import { MAIN_ACTION, REPO, vars } from './ui';

const pos = (left: number, top: number, width: number, height?: number) =>
  vars({ left: `${left}%`, top: `${top}%`, width: `${width}%`, ...(height ? { height: `${height}%` } : {}) });

/** Dealt hero: triptych. The collage cycles three sets of the tool's own output (layout.js runs the crossfade). */
export default function Hero() {
  return (
    <section className="hero hero--triptych" data-enter="" aria-labelledby="hero-h">
      <div className="wrap">
        <div className="hero-text grid hero__grid">
          <div className="span-3">
            <h1 id="hero-h">System-call budgets for Linux jobs</h1>
            <p className="hero__lead">
              Stops one busy job from slowing every other job on a Linux machine: each job gets a budget for expensive system calls. Linux 5.5 and up, no kernel module.
            </p>
          </div>

          <div
            className="ly-collage span-6 start-4"
            role="img"
            aria-label="sgctl status listing two jobs and their budgets, sgd's summary of a finished job, a loan between two jobs in the event log, and calibrated syscall costs"
          >
            <div className="ly-collage__set is-on">
              <div className="ly-piece pc-term" style={pos(4, 6, 74)}>
                <span className="cmd">$ <b>./build/sgctl status</b></span>
                <table>
                  <tbody>
                    <tr><th>JOB</th><th>NAME</th><th>PRIO</th><th>RATE/S</th><th className="hide-sm">WAITED</th><th className="hide-sm">REJECTED</th></tr>
                    <tr><td>1</td><td>interactive</td><td>8</td><td>3200</td><td className="hide-sm">38</td><td className="hide-sm ok">0</td></tr>
                    <tr><td>2</td><td>logger</td><td>2</td><td>800</td><td className="hide-sm">45</td><td className="hide-sm ok">0</td></tr>
                  </tbody>
                </table>
              </div>
              <div className="ly-piece pc-card" style={pos(62, 58, 34, 38)}>
                <span>Global budget</span>
                <span className="big">3,000 units/s</span>
                <span className="mono">sgd -r 3000</span>
              </div>
              <div className="ly-piece pc-term" style={pos(4, 52, 54)}>
                <span className="cmd">$ <b>./build/sgd -r 3000 -l events.jsonl &amp;</b></span>
                <span className="ln">job 2 (logger) ended after 1.778s:</span>
                <span className="ln">calls=8067 allowed=8067</span>
                <span className="ln"><span className="warn">waited=168</span> <span className="ok">rejected=0</span></span>
                <span className="ln">max_wait_ms=9.4</span>
              </div>
            </div>

            <div className="ly-collage__set" aria-hidden="true">
              <div className="ly-piece pc-term" style={pos(4, 8, 70)}>
                <span className="cmd">$ <b>tail events.jsonl</b></span>
                <span className="ln">{'{"ev":"loan_open","loan":1,"from":1,"to":2,'}</span>
                <span className="ln">&nbsp;&quot;amount&quot;:<span className="warn">264.987</span>,&quot;repaid&quot;:0.000,&quot;age_us&quot;:3{'}'}</span>
                <span className="ln">{'{"ev":"loan_repaid","loan":1,"from":1,"to":2,'}</span>
                <span className="ln">&nbsp;&quot;amount&quot;:264.987,&quot;repaid&quot;:<span className="ok">264.987</span>,&quot;age_us&quot;:532769{'}'}</span>
              </div>
              <div className="ly-piece pc-card" style={pos(56, 50, 40, 46)}>
                <span>Fork policy</span>
                <span className="opts">
                  <span className="on">brief <span>waits 500 ms</span></span>
                  <span>wait <span>up to T_max</span></span>
                  <span>reject <span>EAGAIN at once</span></span>
                </span>
              </div>
            </div>

            <div className="ly-collage__set" aria-hidden="true">
              <div className="ly-piece pc-term" style={pos(4, 6, 60)}>
                <span className="cmd">$ <b>./build/sgd -L -c costs/wsl2-6.6.conf</b></span>
                <table>
                  <tbody>
                    <tr><th>NAME</th><th>SEED</th><th>COST</th></tr>
                    <tr><td>fsync</td><td>20</td><td className="warn">1504</td></tr>
                    <tr><td>vfork</td><td>30</td><td>181</td></tr>
                    <tr><td>execve</td><td>40</td><td>112</td></tr>
                    <tr><td>mkdir</td><td>3</td><td>71</td></tr>
                    <tr><td>openat</td><td>3</td><td>4</td></tr>
                  </tbody>
                </table>
              </div>
              <div className="ly-piece pc-term" style={pos(40, 62, 56, 34)}>
                <span className="cmd">$ <b>./build/sgctl metrics</b></span>
                <span className="ln">sg_job_waiting_calls{'{job="2",name="logger"}'} 1</span>
                <span className="ln">sg_job_owed_units{'{job="2",name="logger"}'} 0</span>
              </div>
            </div>
          </div>

          <div className="span-3 start-10 hero__cta">
            <a className="btn btn--primary" href={REPO}>{MAIN_ACTION}</a>
          </div>
        </div>
      </div>
    </section>
  );
}
