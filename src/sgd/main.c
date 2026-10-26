/*
 * sgd: the SyscallGuard supervisor.
 *
 * Each job registered by sg-launch brings a seccomp listener fd; sgd waits
 * on all of them, the registration socket, a 10 ms tick and the metrics
 * port in one epoll loop.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <unistd.h>

#include "common.h"
#include "costs.h"
#include "sgd.h"

struct sgd_config cfg = {
    .global_rate = 200000,
    .burst_ms = 1000,
    .tmax_ms = 2000,
    .fork_wait_ms = 500,
    .aging_ms = 500,
    .qmax = 256,
    .reject_delay_ms = 10,
    .spin_us = 0,
    .sync_wake = 1,
    .borrow = {
        .enabled = 1,
        .reserve_pct = 30,
        .cap_pct = 50,
        .repay_pct = 50,
        .idle_us = 1000000,
        .chunk_pct = 10,
    },
};

struct sgd_state S;

static enum ep_kind tag_listen = EP_LISTEN, tag_timer = EP_TIMER, tag_signal = EP_SIGNAL,
                    tag_http = EP_HTTP_LISTEN;

void die(const char *what)
{
    perror(what);
    exit(1);
}

void ep_add(int fd, void *tag)
{
    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = tag };

    if (epoll_ctl(S.ep, EPOLL_CTL_ADD, fd, &ev) < 0)
        die("epoll_ctl");
}

double units(int64_t milli)
{
    return (double)milli / SG_MILLI;
}

/* Blocking write with a 1 s timeout, so a stuck client cannot stall sgd for long. */
void send_all(int fd, const char *buf, size_t len)
{
    struct timeval tv = { .tv_sec = 1 };
    int flags = fcntl(fd, F_GETFL);

    if (flags >= 0)
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    while (len > 0) {
        ssize_t n = write(fd, buf, len);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        buf += n;
        len -= (size_t)n;
    }
}

static void on_listen(void)
{
    struct conn *c;
    int fd = accept4(S.listen_fd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);

    if (fd < 0)
        return;
    c = calloc(1, sizeof *c);
    if (!c)
        die("calloc");
    c->kind = EP_CONN;
    c->fd = fd;
    ep_add(fd, c);
}

static void on_conn(struct conn *c)
{
    union {
        struct sg_register reg;
        struct sg_ctl ctl;
    } msg;
    struct sg_ack ack = { 0, 0 };
    int lfd;
    ssize_t n = sg_recv_fd(c->fd, &msg, sizeof msg, &lfd);

    if (n < 0 && (errno == EAGAIN || errno == EINTR))
        return;

    if (n == (ssize_t)sizeof msg.ctl && msg.ctl.magic == SG_CTL_MAGIC) {
        if (lfd >= 0)
            close(lfd);
        ctl_handle(c->fd, &msg.ctl);
        goto done;
    }

    if (n != (ssize_t)sizeof msg.reg || msg.reg.magic != SG_PROTO_MAGIC || lfd < 0)
        ack.status = -EPROTO;
    else if (msg.reg.prio < SG_PRIO_MIN || msg.reg.prio > SG_PRIO_MAX || msg.reg.rate < 0 ||
             msg.reg.fork_policy < 0 || msg.reg.fork_policy >= SG_FORK_COUNT)
        ack.status = -EINVAL;

    if (ack.status == 0)
        ack.job_id = job_new(&msg.reg, lfd)->acct.id;
    else if (lfd >= 0)
        close(lfd);   /* the launcher's costed calls now fail with ENOSYS */

    if (write(c->fd, &ack, sizeof ack) < 0)
        perror("sgd: ack");
done:
    close(c->fd);
    free(c);
}

static int open_listener(const char *path)
{
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    mode_t old_mask;
    int fd;

    if (strlen(path) >= sizeof addr.sun_path) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strcpy(addr.sun_path, path);

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0) {
        fprintf(stderr, "sgd: another sgd is already listening on %s\n", path);
        exit(1);
    }
    close(fd);
    unlink(path);

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0)
        return -1;
    old_mask = umask(077);   /* only this user may register jobs or send commands */
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        umask(old_mask);
        return -1;
    }
    umask(old_mask);
    if (listen(fd, 64) < 0)
        return -1;
    return fd;
}

static void shutdown_all(void)
{
    release_all_calls();
    while (S.jobs) {
        fprintf(stderr, "[sgd] warning: job %u (%s) is still running; its costed calls will now fail with ENOSYS\n",
                S.jobs->acct.id, S.jobs->name);
        job_free(S.jobs);
    }
    close(S.listen_fd);
    if (S.http_fd >= 0)
        close(S.http_fd);
    unlink(cfg.sock);
    global_refill(sg_now_us());
    log_sgd("sgd_stop");
    fprintf(stderr, "[sgd] stopped: loans opened=%llu repaid=%llu written_off=%llu\n", S.ledger.opened,
            S.ledger.repaid, S.ledger.written_off);
    if (cfg.log)
        fclose(cfg.log);
    sg_waitq_free(&S.wq);
    free(S.scratch);
    free(S.rejects);
    free(S.accts);
    free(S.req);
    free(S.resp);
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [-s sock] [-r rate] [-H burst_ms] [-T tmax_ms] [-F fork_wait_ms] [-a aging_ms] [-q qmax]\n"
            "          [-I idle_ms] [-B] [-k chunk_pct] [-R delay_ms] [-S spin_us] [-W]\n"
            "          [-m port] [-c costs] [-L] [-l log]\n"
            "  -s sock      listening socket (default /tmp/syscallguard-<uid>.sock)\n"
            "  -r rate      global budget R_g in cost units/s (default 200000)\n"
            "  -H burst_ms  burst horizon H: capacity = rate * H (default 1000)\n"
            "  -T tmax_ms   reject a call that has waited this long (default 2000)\n"
            "  -F ms        process creation of a job with fork policy \"brief\" waits at most this\n"
            "               long, then fails with EAGAIN (default 500; see sg-launch -f)\n"
            "  -a aging_ms  waiting this long raises effective priority by 1 (default 500)\n"
            "  -q qmax      max waiting calls per job (default 256)\n"
            "  -I idle_ms   a job idle this long may lend to any priority (default 1000)\n"
            "  -B           disable borrowing (baseline for evaluation)\n"
            "  -k pct       borrow at least pct%% of the borrower's capacity per loan (default 10; 0 = deficit)\n"
            "  -R delay_ms  hold budget rejections this long before replying (default 10; 0 = at once)\n"
            "  -S spin_us   keep polling this long after a notification before sleeping (default 0;\n"
            "               fastest round trip but burns CPU; turns synchronous wake-up off)\n"
            "  -W           do not request synchronous wake-ups (Linux 6.6+)\n"
            "  -m port      serve Prometheus metrics on http://127.0.0.1:port/metrics\n"
            "  -c costs     load calibrated syscall costs (from tools/sg_calibrate.py)\n"
            "  -L           print the cost table in effect and exit\n"
            "  -l log       write a JSON-lines event log\n",
            argv0);
    exit(2);
}

static int64_t parse_min(const char *s, long long min, const char *argv0)
{
    char *end;
    long long v;

    errno = 0;
    v = strtoll(s, &end, 10);
    if (errno || *s == '\0' || *end != '\0' || v < min)
        usage(argv0);
    return v;
}

static int64_t parse_pos(const char *s, const char *argv0)
{
    return parse_min(s, 1, argv0);
}

int main(int argc, char **argv)
{
    struct itimerspec tick = {
        .it_interval = { 0, TICK_MS * 1000000L },
        .it_value = { 0, TICK_MS * 1000000L },
    };
    sigset_t sigs;
    char err[256];
    int opt, list_costs = 0;

    S.http_fd = -1;
    sg_default_sock(cfg.sock, sizeof cfg.sock);
    while ((opt = getopt(argc, argv, "s:r:H:T:F:a:q:I:Bk:R:S:Wm:c:Ll:h")) != -1) {
        switch (opt) {
        case 's': snprintf(cfg.sock, sizeof cfg.sock, "%s", optarg); break;
        case 'r': cfg.global_rate = parse_pos(optarg, argv[0]); break;
        case 'H': cfg.burst_ms = parse_pos(optarg, argv[0]); break;
        case 'T': cfg.tmax_ms = parse_pos(optarg, argv[0]); break;
        case 'F': cfg.fork_wait_ms = parse_pos(optarg, argv[0]); break;
        case 'a': cfg.aging_ms = parse_pos(optarg, argv[0]); break;
        case 'q': cfg.qmax = (unsigned)parse_pos(optarg, argv[0]); break;
        case 'I': cfg.borrow.idle_us = parse_pos(optarg, argv[0]) * 1000; break;
        case 'B': cfg.borrow.enabled = 0; break;
        case 'k':
            cfg.borrow.chunk_pct = parse_min(optarg, 0, argv[0]);
            if (cfg.borrow.chunk_pct > 100)
                usage(argv[0]);
            break;
        case 'R': cfg.reject_delay_ms = parse_min(optarg, 0, argv[0]); break;
        case 'S': cfg.spin_us = parse_min(optarg, 0, argv[0]); break;
        case 'W': cfg.sync_wake = 0; break;
        case 'm':
            cfg.metrics_port = (int)parse_pos(optarg, argv[0]);
            if (cfg.metrics_port > 65535)
                usage(argv[0]);
            break;
        case 'c':
            if (sg_load_costs(optarg, err, sizeof err) < 0) {
                fprintf(stderr, "sgd: %s\n", err);
                return 2;
            }
            cfg.cost_file = optarg;
            break;
        case 'L':
            list_costs = 1;
            break;
        case 'l':
            cfg.log = fopen(optarg, "w");
            if (!cfg.log)
                die(optarg);
            break;
        default: usage(argv[0]);
        }
    }
    if (optind != argc)
        usage(argv[0]);
    if (list_costs) {
        sg_dump_costs(stdout);
        return 0;
    }
    /* Synchronous wake-up moves threads onto sgd's CPU, where a spinning sgd never yields. */
    if (cfg.spin_us && cfg.sync_wake) {
        fprintf(stderr, "[sgd] note: spinning (-S) and synchronous wake-up compete for one CPU; "
                        "turning synchronous wake-up off\n");
        cfg.sync_wake = 0;
    }

    if (syscall(SYS_seccomp, SECCOMP_GET_NOTIF_SIZES, 0, &S.sz) < 0)
        die("sgd: SECCOMP_GET_NOTIF_SIZES");
    S.req = malloc(S.sz.seccomp_notif);
    S.resp = malloc(S.sz.seccomp_notif_resp);
    if (!S.req || !S.resp)
        die("malloc");
    S.ledger.log = log_loan;

    signal(SIGPIPE, SIG_IGN);
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &sigs, NULL) < 0)
        die("sigprocmask");
    S.signal_fd = signalfd(-1, &sigs, SFD_CLOEXEC);
    S.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    S.ep = epoll_create1(EPOLL_CLOEXEC);
    if (S.signal_fd < 0 || S.timer_fd < 0 || S.ep < 0)
        die("sgd: setup");
    if (timerfd_settime(S.timer_fd, 0, &tick, NULL) < 0)
        die("timerfd_settime");
    S.listen_fd = open_listener(cfg.sock);
    if (S.listen_fd < 0)
        die(cfg.sock);
    if (cfg.metrics_port) {
        S.http_fd = http_open(cfg.metrics_port);
        if (S.http_fd < 0)
            die("sgd: metrics port");
        ep_add(S.http_fd, &tag_http);
    }

    S.start_us = sg_now_us();
    sg_bucket_init(&S.global, cfg.global_rate, cfg.global_rate * cfg.burst_ms, S.start_us);
    S.global.tokens = S.global.capacity;
    S.global_entitled = S.global.capacity;

    ep_add(S.listen_fd, &tag_listen);
    ep_add(S.timer_fd, &tag_timer);
    ep_add(S.signal_fd, &tag_signal);
    log_sgd("sgd_start");
    fprintf(stderr,
            "[sgd] listening on %s: global %lld units/s, burst %lld ms, T_max %lld ms, borrowing %s, %s costs\n",
            cfg.sock, (long long)cfg.global_rate, (long long)cfg.burst_ms, (long long)cfg.tmax_ms,
            cfg.borrow.enabled ? "on" : "off", cfg.cost_file ? cfg.cost_file : "seed");
    if (cfg.metrics_port)
        fprintf(stderr, "[sgd] metrics on http://127.0.0.1:%d/metrics\n", cfg.metrics_port);
    fprintf(stderr,
            "[sgd] brief fork wait %lld ms, reject delay %lld ms, loan chunk %lld%%, spin %lld us, "
            "synchronous wake-up %s\n",
            (long long)cfg.fork_wait_ms, (long long)cfg.reject_delay_ms, (long long)cfg.borrow.chunk_pct,
            (long long)cfg.spin_us, cfg.sync_wake ? "requested" : "off");

    for (;;) {
        struct epoll_event evs[64];
        int timeout = cfg.spin_us && sg_now_us() - S.last_notify_us < cfg.spin_us ? 0 : -1;
        int n = epoll_wait(S.ep, evs, 64, timeout), i;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            die("epoll_wait");
        }
        for (i = 0; i < n; i++) {
            void *tag = evs[i].data.ptr;

            switch (*(enum ep_kind *)tag) {
            case EP_LISTEN:
                on_listen();
                break;
            case EP_CONN:
                on_conn(tag);
                break;
            case EP_HTTP_LISTEN:
                http_on_accept();
                break;
            case EP_HTTP:
                http_on_request(tag);
                break;
            case EP_TIMER:
                on_tick();
                break;
            case EP_SIGNAL:
                shutdown_all();
                return 0;
            case EP_JOB:
                if (evs[i].events & EPOLLIN)
                    on_notify(tag);
                else if (evs[i].events & (EPOLLHUP | EPOLLERR))
                    end_job(tag);
                break;
            }
        }
    }
}
