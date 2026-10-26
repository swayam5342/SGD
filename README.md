# SyscallGuard

Cost-based system-call budgets with priority-aware borrowing, built on
seccomp user notification. Runs on an unmodified Linux kernel (5.5+, 5.19+
recommended, 6.6+ for synchronous wake-up); no kernel module, and no root
needed for enforcement. x86-64 and aarch64.

A job started with `sg-launch` gets a seccomp filter that lets cheap calls
through inside the kernel and sends costed ones (I/O, metadata, fsync,
mmap, process creation, exec, socket setup) to the `sgd` supervisor. sgd
charges each call against the job's token bucket and a global one, and
allows it, holds it until budget is available, or rejects it with an errno.

## Build and test

```sh
make            # builds sgd, sg-launch, sgctl and notify_openat into build/
make test       # unit, Python and integration tests
```

Needs `gcc` and `make` (`sudo apt install build-essential`) and `python3`
for the tools.

## Usage

Start the supervisor, then run jobs under it:

```sh
./build/sgd -r 3000 -l events.jsonl &              # global budget: 3000 units/s
./build/sg-launch -p 8 -n build -- make -j4         # high priority job
./build/sg-launch -p 2 -n logger -- ./noisy-logger  # low priority job
./build/sg-launch -r 500 -- ./batch                 # fixed 500 units/s instead of a share
./build/sg-launch -f reject -- ./untrusted          # fork fails at once when over budget
```

`-f` / `--fork-policy` sets what a job's process creation (fork, vfork,
non-thread clone) does when the job is short of budget:

| policy | behaviour |
|---|---|
| `brief` (default) | wait for budget up to `sgd -F` (500 ms), then fail with EAGAIN: a throttled build slows down, a fork storm still fails |
| `wait` | wait like any other call, up to T_max |
| `reject` | fail with EAGAIN at once |

When each job ends, sgd prints a summary:

```
[sgd] job 1 (logger) ended after 1.778s: calls=8067 allowed=8067 waited=168 rejected=0 dropped=0 charged=8315 max_wait_ms=9.4
```

| sgd option | Meaning | Default |
|---|---|---|
| `-r rate` | global budget R_g, cost units/s | 200000 |
| `-H ms` | burst horizon H (bucket capacity = rate x H) | 1000 |
| `-T ms` | T_max: a call waiting longer is rejected with EAGAIN | 2000 |
| `-F ms` | process creation of a job with fork policy `brief` waits at most this long | 500 |
| `-a ms` | aging: waiting this long raises priority by one level | 500 |
| `-q n` | waiting calls allowed per job | 256 |
| `-I ms` | idle window: an idle job may lend to any priority | 1000 |
| `-B` | disable borrowing | on |
| `-k pct` | borrow at least pct% of the borrower's capacity per loan (0 = exact deficit) | 10 |
| `-R ms` | hold budget rejections this long before replying (0 = at once) | 10 |
| `-S us` | spin this long after a notification (turns synchronous wake-up off) | 0 |
| `-W` | do not request synchronous wake-ups (Linux 6.6+) | requested |
| `-c file` | load calibrated syscall costs | seed costs |
| `-L` | print the cost table in effect (seed and current) and exit | |
| `-m port` | serve Prometheus metrics on `http://127.0.0.1:port/metrics` | off |
| `-l file` | JSON-lines event log (every decision, per-job and per-process stats) | off |
| `-s path` | socket | `/tmp/syscallguard-<uid>.sock` |

## Inspecting and adjusting a running sgd

```sh
./build/sgctl status              # global bucket, totals, one row per job
./build/sgctl loans               # outstanding loans
./build/sgctl metrics             # Prometheus text (same as the HTTP endpoint)
./build/sgctl set-prio 2 9        # change a job's priority at run time
./build/sgctl set-rate 2 500      # fixed 500 units/s; 0 returns it to a priority share
./build/sgctl kill 2              # SIGKILL the job's whole process group
./build/sgctl kill 2 15           # or a chosen signal (15 = SIGTERM)
```

`kill` is the hard stop for a runaway job: budgets bound how fast a job
spends, but a job that keeps spawning (a fork storm) still grows up to the
system's process limit. sg-launch puts each job in its own process group, so
`sgctl kill` signals the whole tree at once; when the processes exit, sgd
tears the job down as usual.

```
JOB  NAME           PRIO  MODE   RATE/S     TOKENS   CAPACITY WAIT    CALLS  ALLOWED  WAITED REJECTED     OWED     LENT   FORK
1    interactive       8 share     3200        0.4     3200.0    1     4467     4466      38        0      0.0      0.0  brief
2    logger            2 share      800        0.8      800.0    1      928      927      45        0      0.0      0.0  brief
```

## Calibrating costs (needs root)

The seed costs in [src/costs.c](src/lib/costs.c) only rank the classes.
`tools/sg_calibrate.py` measures real ones with eBPF: bpftrace records a
latency histogram per syscall on the `raw_syscalls` tracepoints while a
workload runs, and the median of each costed call becomes its cost
(1 unit = 1 us, minimum 1). Thread creation is measured separately as
`clone_thread`.

```sh
sudo apt install bpftrace
make calibrate                                             # built-in workload, writes costs/$(uname -r).conf
sudo python3 tools/sg_calibrate.py -o costs/mine.conf -- make -j4   # or your own workload
./build/sgd -L -c costs/mine.conf                          # check: seed vs calibrated cost
./build/sgd -c costs/mine.conf ...                         # enforce with calibrated costs
```

Calibrated on WSL2 (kernel 6.6, [costs/wsl2-6.6.conf](costs/wsl2-6.6.conf));
two runs agreed within 3%:

| syscall | seed | calibrated | | syscall | seed | calibrated |
|---|---|---|---|---|---|---|
| fsync | 20 | 1504 | | mkdir | 3 | 71 |
| vfork | 30 | 181 | | clone_thread | 10 | 45 |
| clone (process) | 30 | 167 | | openat | 3 | 4 |
| fdatasync | 20 | 167 | | write | 1 | 3 |
| execve | 40 | 112 | | read | 1 | 1 |

Latency is measured from syscall entry to exit, so it includes any time the
call blocks; the median and a non-blocking workload keep that small. A
calibration file belongs to the machine (and filesystem) it was measured on.

## Analysing a run

`tools/sg_analyze.py` turns the event log into metrics
(standard library only; `--plot` needs `python3-matplotlib`):

```sh
python3 tools/sg_analyze.py events.jsonl              # text summary
python3 tools/sg_analyze.py events.jsonl --jobs 1,2   # only jobs that ran together
python3 tools/sg_analyze.py events.jsonl --json       # for further processing
python3 tools/sg_analyze.py events.jsonl --plot waits.png
```

It gives per job and overall: admission wait p50/p95/p99/max, waited and
rejected percentages, rejection rate per syscall class, budget utilisation
(charged / entitled), loans (count, volume, repayment ratio, time to repay)
and Jain's fairness index over charged units per second per priority weight
(1.0 = service exactly proportional to priority). Utilisation and fairness
are computed over each job's (and sgd's) whole lifetime, so compare jobs
that ran over the same period.

## Borrowing

When a call costs more than the job holds, sgd tries to borrow before making
it wait (only if the global bucket could then admit it):

- the lender is not the borrower, has no waiting calls and owes nothing itself
- the lender keeps a reserve of 30% of its capacity
- the borrower may owe at most 50% of its capacity
- lenders have equal or lower priority, unless idle for the idle window
- the eligible lender with the largest surplus wins, ties to lower priority
- a loan is at least 10% of the borrower's capacity (`-k`), so a throttled
  job takes a few larger loans instead of one per call

Tokens move from lender to borrower, so none are created. Half of every
later refill of the borrower repays its oldest loan. Loans are written off
when either side exits. Each loan's open, repaid and written-off events go
to the event log:

```
{"ev":"loan_open","loan":1,"from":1,"to":2,"amount":264.987,"repaid":0.000,"age_us":3}
{"ev":"loan_repaid","loan":1,"from":1,"to":2,"amount":264.987,"repaid":264.987,"age_us":532769}
```

Because loans are repaid from later refills, borrowing absorbs a burst
rather than raising a job's sustained rate: it can shorten a throttled job
by at most (borrower cap) x H, 0.5 s with the defaults.

## Layout

| Path | Contents |
|---|---|
| [src/lib/](src/lib/) | core library (`libsg.a`) shared by all programs |
| [src/lib/costs.c](src/lib/costs.c) | cost table and seccomp filter generator |
| [src/lib/bucket.c](src/lib/bucket.c) | token bucket |
| [src/lib/waitq.c](src/lib/waitq.c) | waiting queue (heap) |
| [src/lib/borrow.c](src/lib/borrow.c) | borrowing and loan ledger |
| [src/lib/proto.c](src/lib/proto.c) | registration and control messages, fd passing |
| [src/sgd/main.c](src/sgd/main.c) | supervisor: options, event loop, registration socket |
| [src/sgd/admit.c](src/sgd/admit.c) | allow / wait / reject decisions, waiting calls, paced rejections |
| [src/sgd/job.c](src/sgd/job.c) | job lifecycle, priority shares, per-process stats |
| [src/sgd/log.c](src/sgd/log.c) | JSON-lines event log |
| [src/sgd/ctl.c](src/sgd/ctl.c) | sgctl commands |
| [src/sgd/metrics.c](src/sgd/metrics.c) | Prometheus metrics and the HTTP endpoint |
| [src/sg-launch.c](src/sg-launch.c) | job launcher (installs the filter, hands the listener to sgd) |
| [src/sgctl.c](src/sgctl.c) | control client |
| [examples/notify_openat.c](examples/notify_openat.c) | standalone example: log every openat() through user notification |
| [tools/sg_analyze.py](tools/sg_analyze.py) | metrics from the event log |
| [tools/sg_calibrate.py](tools/sg_calibrate.py) | eBPF cost calibration |
| [tests/](tests/) | unit, Python and integration tests |

## Design notes and limitations

- **Job = listener.** Each `sg-launch` installs its own filter, so the fd a
  notification arrives on identifies the job; no pid-to-job lookup.
- **Aging without re-keying.** Effective priority `prio + wait/q` orders the
  same as the fixed key `prio*q - arrival`, so the heap never needs rebuilding.
- **Per-job FIFO.** A job's later call never overtakes its earlier waiting
  call; a waiter held back by the shared global bucket blocks lower-priority
  arrivals (strict priority on the global bucket).
- **clone3 returns ENOSYS** inside the kernel, so glibc falls back to
  `clone()`, whose flags are in a register: threads are priced separately and
  wait, processes follow the job's fork policy.
- **io_uring_setup returns EPERM**, because one `io_uring_enter` could submit
  many operations and bypass per-call accounting.
- **sendmsg/recvmsg stay on the fast path** (uncharged): sg-launch needs
  sendmsg to hand over the listener before any costed call is possible.
- **Costs are charged before the call runs**, even if it then fails (e.g. a
  failed `execve` during a PATH search).
- **If sgd exits**, costed calls of still-running jobs fail with ENOSYS; the
  filter cannot be removed from a running process.
- **The kernel releases a filter only when the process is reaped**, so a job
  ends (listener POLLHUP) only once its processes have been waited for.
- **Synchronous wake-up** (Linux 6.6) speeds up the reply to a job, but sgd
  itself sleeps in epoll, which does not pass the same-CPU hint on; under
  very heavy notification load it can drag sgd across CPUs.
- The control socket is private to the user (mode 0600); the metrics port
  listens on 127.0.0.1 only but is readable by every local user.
- Use a VM or a spare machine, not a shared host, for fork-storm experiments.

## License

MIT, see [LICENSE](LICENSE).
