import { vars } from './ui';

const DECIDE = {
  layout: 'lanes',
  curve: 'curve',
  alt: 'An fsync call from the logger job goes to sgd, which allows it, holds it until budget refills, or rejects it with EAGAIN.',
  input: { label: 'fsync()', icon: 'ui:file-code', meta: 'cost 20' },
  core: { label: 'sgd', icon: 'ui:cpu' },
  lanes: [
    { label: 'Allowed', icon: 'ui:check', meta: 'budget left', done: true },
    { label: 'Held', icon: 'ui:clock', meta: 'until refill' },
    { label: 'Rejected', icon: 'ui:x', meta: 'EAGAIN' },
  ],
};

const LOAN = {
  layout: 'sync',
  alt: 'Job 1, interactive, lends 264.987 units to job 2, logger; the loan is repaid from later refills.',
  left: { label: 'interactive', icon: 'ui:terminal', meta: 'prio 8', rows: [{ label: 'Capacity', meta: '3200' }, { label: 'Reserve', meta: '30%' }, { label: 'Lent', meta: '264.987' }] },
  link: { label: 'Loan 1', icon: 'ui:refresh-cw' },
  right: { label: 'logger', icon: 'ui:terminal', meta: 'prio 2', rows: [{ label: 'Capacity', meta: '800' }, { label: 'Owed', meta: '264.987' }, { label: 'Repaid', meta: 'in 0.53 s' }] },
};

// The card group's still ground (painterly-landscape, dealt), one seed and one lit palette per panel.
const ground = (seed: number, palette: object) => ({
  'data-fx': 'painterly-landscape',
  'data-fx-seed': seed,
  'data-fx-palette': JSON.stringify(palette),
  'data-fx-params': JSON.stringify({ blur: 1, brush: 0.35, slope: 0, hillY: 0.3, horizon: 0.62, speed: 0, drift: 0 }),
});
const SAGE = { bg: '#cdd6c6', surface: '#dce3d6', ink: '#262d24', accent: '#6c8e63' };
const SAND = { bg: '#d9ccb7', surface: '#e9e0d2', ink: '#2a2622', accent: '#a8794a' };

// sgd's defaults, from the option table in the README and the usage text in src/sgd/main.c.
const OPTIONS = [
  { label: 'Borrowing', sub: 'on; -B turns it off', on: true },
  { label: 'Paced rejections', sub: 'held 10 ms; -R 0 replies at once', on: true },
  { label: 'Synchronous wake-up', sub: 'requested on Linux 6.6+; -W turns it off', on: true },
  { label: 'Prometheus metrics', sub: 'off; -m port serves 127.0.0.1', on: false },
  { label: 'Event log', sub: 'off; -l file writes JSON lines', on: false },
];

// What sgctl and sg-launch do, as the README's comments describe each command.
const STOPS = [
  { title: 'sgctl kill 2', body: 'SIGKILL to the job’s whole process group' },
  { title: 'sgctl kill 2 15', body: 'Or a chosen signal (15 = SIGTERM)' },
  { title: 'sg-launch -f reject', body: 'Fork fails with EAGAIN at once when over budget' },
];

function Card({ title, body, children }: { title: string; body: string; children: React.ReactNode }) {
  return (
    <article className="card card--row card--open span-6">
      <h3>{title}</h3>
      <p>{body}</p>
      {children}
    </article>
  );
}

/** features-2x2 as two screens (.g2): row 1 holds the h2 and two cards, row 2 the other two. */
export default function Features() {
  return (
    <section className="section g2" id="features" aria-labelledby="features-h">
      <div className="wrap">
        <div className="group">
          <div className="section-head">
            <h2 id="features-h">
              What sgd does with a costly call. <span className="muted">It charges the job, then decides.</span>
            </h2>
          </div>
          <div className="grid">
            <Card title="sgd allows, holds or rejects calls" body="Within budget the call runs. Short of it, the call waits up to 2 s, then fails with EAGAIN.">
              <div className="panel" data-ui-loop="">
                <div className="dg" data-diagram={JSON.stringify(DECIDE)} />
              </div>
            </Card>
            <Card title="sgd lends budget between jobs" body="An idle job lends to a short one, keeping 30% in reserve. Half of each later refill repays the loan.">
              <div className="panel">
                <div className="dg" data-diagram={JSON.stringify(LOAN)} />
              </div>
            </Card>
          </div>
        </div>

        <div className="grid">
          <Card title="SyscallGuard policies are sgd options" body="Borrowing, paced rejections and synchronous wake-up are on by default; one flag on sgd turns each off.">
            <div className="panel" {...ground(31, SAGE)}>
              <div className="stage">
                <div className="at-right" style={vars({ '--ax': '32px', '--ay': '40px' })}>
                  <div className="ui ui-panel ui-toggles" data-ui="toggles">
                    <div className="ui-head">
                      <svg className="ui-icon ui-toggles__ico" viewBox="0 0 16 16" aria-hidden="true">
                        <path d="M8 1.8 13 4v3.6c0 3-2.1 5.4-5 6.6-2.9-1.2-5-3.6-5-6.6V4z" />
                        <path d="m5.8 8 1.6 1.6 3-3.2" />
                      </svg>
                      <div className="ui-title">sgd defaults</div>
                      <span className="ui-spacer" />
                      <div className="ui-meta" data-tg-count="">3 of 5 on</div>
                    </div>
                    <ul className="ui-toggles__list">
                      {OPTIONS.map((o) => (
                        <li key={o.label} className="ui-tg">
                          <span className="ui-tg__text">
                            <span className="ui-tg__label">{o.label}</span>
                            <span className="ui-tg__sub">{o.sub}</span>
                          </span>
                          <button className={`ui-switch${o.on ? ' is-on' : ''}`} type="button" role="switch" aria-checked={o.on} aria-label={o.label}>
                            <i />
                          </button>
                        </li>
                      ))}
                    </ul>
                  </div>
                </div>
              </div>
            </div>
          </Card>

          <Card title="sgctl kill stops a runaway job" body="sgctl kill signals the job's whole process group, so a fork storm ends in one command.">
            <div className="panel" {...ground(47, SAND)}>
              <div className="stage stage--fade-b">
                <div
                  className="at-rise fit"
                  style={vars({ '--fit-max': '420px', '--ax': 'max(24px, 50% - 210px)', '--ay': 'calc(100% - 200px)', '--ay-sm': '24px' })}
                >
                  <div className="ui ui-notify" data-ui="notify">
                    <ul className="ui-notify__list" data-notify-list="">
                      {STOPS.map((n) => (
                        <li key={n.title} className="ui-note">
                          <span className="ui-note__ico"><svg className="ui-icon" viewBox="0 0 16 16"><path d="M4 4l8 8M12 4l-8 8" /></svg></span>
                          <span className="ui-note__text">
                            <span className="ui-note__title">{n.title}</span>
                            <span className="ui-note__body">{n.body}</span>
                          </span>
                        </li>
                      ))}
                    </ul>
                  </div>
                </div>
              </div>
            </div>
          </Card>
        </div>
      </div>
    </section>
  );
}
