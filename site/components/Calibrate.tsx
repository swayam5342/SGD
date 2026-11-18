import { REPO, vars } from './ui';

// Lines copied from costs/wsl2-6.6.conf as tools/sg_calibrate.py wrote them (a subset, in file order).
// name, cost and seed of a line, for the phone's narrower panel
const short = (l: string) => { const [n, cost, , , seed] = l.replace(/^#\s*/, '').trim().split(/\s+/); return `${n.padEnd(13)}${cost.padStart(5)}${seed.padStart(6)}`; };
const HEADER = "# name             cost  median_ns   samples  seed";
const COSTS: { line: string; mark?: boolean; note?: string }[] = [
  { line: "read                  1        533     17121     1" },
  { line: "write                 3       2621     15154     1" },
  { line: "openat                4       3725     31832     3" },
  { line: "mkdir                71      70762     15071     3" },
  { line: "fsync              1504    1504367       954    20", mark: true },
  { line: "fdatasync           167     167185       941    20" },
  { line: "vfork               181     180972       470    30", note: 'vfork: 181 units, seed 30.' },
  { line: "clone               167     167184       470    30" },
  { line: "execve              112     111550       470    40", note: 'execve: 112 units, seed 40.' },
  { line: "clone_thread         45      45426       472    10" },
];

/** The dealt moment (liquid-type) on a statement of its own. */
export function Statement() {
  return (
    <section className="section statement" id="measured" aria-labelledby="measured-h">
      <div className="wrap">
        <div data-moment="liquid-type" className="section-head" style={{ maxWidth: 'none' }}>
          <h2 className="mo-lt__type" id="measured-h">Syscall costs measured on your kernel</h2>
          <p className="t-lead">
            The seed costs only rank the call classes. sg_calibrate.py times every costly call with bpftrace and writes the median, in microseconds, to a file sgd loads.
          </p>
        </div>
      </div>
    </section>
  );
}

/** split-text-left; its frame is the page's one [data-visual], holding the dealt effect (gradient-mesh). */
export default function Calibrate() {
  return (
    <section className="section" id="calibrate" aria-labelledby="calibrate-h">
      <div className="wrap">
        <div className="grid grid--top">
          <div className="span-5 stack-5">
            <h2 id="calibrate-h">sg_calibrate.py measures costs with eBPF</h2>
            <p className="t-lead">
              Run your own workload under bpftrace. The median latency of each costly call becomes its cost, 1 unit to the microsecond.
            </p>
            <p>On WSL2, kernel 6.6, fsync came out at 1,504 units against a seed of 20. Two runs agreed within 3%.</p>
            <p>Needs root, and the file belongs to the machine it was measured on.</p>
            <a className="link" href={`${REPO}/blob/main/tools/sg_calibrate.py`}>Read sg_calibrate.py</a>
          </div>
          <div className="frame frame--split fill span-6 start-7" data-visual="" data-fx="gradient-mesh" data-fx-seed="12">
            <div className="stage stage--fade-br">
              <div className="at-rise to-edge" style={vars({ '--w': '560px', '--ax': '40px', '--ay': '56px' })} data-ui-loop="">
                <div className="ui ui-panel ui-code" data-ui="code">
                  <div className="ui-code__tabs">
                    <span className="ui-code__tab is-on"><i className="ui-code__lang">conf</i>wsl2-6.6.conf</span>
                    <span className="ui-code__tab"><i className="ui-code__lang">py</i>sg_calibrate.py</span>
                    <span className="ui-spacer" />
                    <span className="ui-chip ui-chip--plain ui-code__chip" data-code-chip="">WSL2</span>
                  </div>
                  <pre className="ui-code__pre">
                    <code>
                      <span className="ui-code__ln" data-n="1"><span className="c"><span className="cc-full">{HEADER}</span><span className="cc-short"># {short(HEADER)}</span></span></span>
                      {'\n'}
                      {COSTS.map((c, i) => (
                        <span key={c.line}>
                          <span className={`ui-code__ln${c.mark ? ' is-mark' : ''}`} data-n={i + 2} data-note={c.note}><span className="cc-full">{c.line}</span><span className="cc-short">{short(c.line)}</span></span>
                          {i < COSTS.length - 1 ? '\n' : null}
                        </span>
                      ))}
                    </code>
                  </pre>
                  <div className="ui-code__hint" data-code-hint="">
                    <span className="ui-dot" />
                    <span data-code-text="">fsync: 1,504 units, seed 20.</span>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
