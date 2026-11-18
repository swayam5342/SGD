import { Dots } from './ui';

// Every line below is from the README's Usage and Inspecting sections, as sgd, sg-launch and sgctl print them.
const LAUNCH = [
  './build/sgd -r 3000 -l events.jsonl &',
  './build/sg-launch -p 8 -n build -- make -j4',
  './build/sg-launch -p 2 -n logger -- ./noisy-logger',
  './build/sg-launch -r 500 -- ./batch',
  './build/sg-launch -f reject -- ./untrusted',
];

// sgctl status (README), columns abridged; the header names are the ones src/sgd/ctl.c prints.
const STATUS = {
  head: ['JOB', 'NAME', 'PRIO', 'MODE', 'RATE/S', 'WAITED', 'REJECTED'],
  rows: [
    ['1', 'interactive', '8', 'share', '3200', '38', '0'],
    ['2', 'logger', '2', 'share', '800', '45', '0'],
  ],
};

// The job summary sgd prints when a job ends (README; format from src/sgd/job.c), split over lines.
// The leading "[sgd] " tag is left off: the kit outlines bracketed text as a leftover placeholder.
const SUMMARY = [
  'job 2 (logger) ended after 1.778s:',
  'calls=8067 allowed=8067 waited=168',
  'rejected=0 dropped=0 charged=8315',
  'max_wait_ms=9.4',
];

/** app-frame: the commands and sgctl status beside the terminal where a job ends. */
export default function Jobs() {
  return (
    <section className="section" id="jobs" aria-labelledby="jobs-h">
      <div className="wrap">
        <div className="grid grid--top">
          <h2 className="span-5" id="jobs-h">How the sgd supervisor works</h2>
          <div className="span-6 start-7 stack-4">
            <p>
              Start sgd once, then start each job with sg-launch. Cheap calls run as usual; each expensive one is charged to its job, and sgd lets it run, makes it wait, or refuses it.
            </p>
            <a className="link" href="#features">See what sgd decides</a>
          </div>
        </div>

        <div
          className="ui app fill"
          role="img"
          aria-label="sgd started with a global budget of 3000 units per second, four jobs launched under it, sgctl status listing two of them, and the summary sgd prints when the logger job ends"
          data-ui-pause-host=""
        >
          <div className="app__pane app__pane--list">
            <div className="pc-term pc-term--pane">
              {LAUNCH.map((l) => (
                <span key={l} className="cmd">$ <b>{l}</b></span>
              ))}
              <span className="cmd">$ <b>./build/sgctl status</b></span>
              <table>
                <tbody>
                  <tr>{STATUS.head.map((h) => <th key={h}>{h}</th>)}</tr>
                  {STATUS.rows.map((r) => (
                    <tr key={r[0]}>{r.map((c, i) => <td key={i}>{c}</td>)}</tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>

          <div className="app__pane app__pane--open" data-ui-loop="">
            <div className="ui ui-panel ui-term" data-ui="terminal-run">
              <div className="ui-term__head">
                <Dots />
                <span className="ui-term__title">SGD</span>
                <span className="ui-meta">bash</span>
              </div>
              <div className="ui-term__screen" data-term-screen="">
                <div className="ui-term__track" data-term-track="">
                  <div className="ui-term__ln ui-term__cmd">
                    <span className="ui-term__cwd">~/SGD</span>
                    <span className="ui-term__sig">$</span>
                    <span className="ui-term__typed" data-term-cmd="">./build/sg-launch -p 2 -n logger -- ./noisy-logger</span>
                  </div>
                  {SUMMARY.map((l) => (
                    <div key={l} className="ui-term__ln" data-term-out="">
                      <span className="ui-term__path">{l}</span>
                    </div>
                  ))}
                  <div className="ui-term__ln ui-term__cmd" data-term-next="">
                    <span className="ui-term__cwd">~/SGD</span>
                    <span className="ui-term__sig">$</span>
                    <span className="ui-term__typed" />
                    <i className="ui-term__caret" aria-hidden="true" />
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
