import { Arrow, MAIN_ACTION, REPO } from './ui';

const COLUMNS = [
  { head: 'Product', links: [['#jobs', 'sgd and sg-launch'], ['#features', 'sgctl']] },
  { head: 'Features', links: [['#features', 'Borrowing'], ['#features', 'Fork policies']] },
  { head: 'Costs', links: [['#measured', 'Seed costs'], ['#calibrate', 'Calibration']] },
  { head: 'Questions', links: [['#questions', 'Limits'], [`${REPO}/issues`, 'Issues']] },
];

/** footer.closing: the dealt close (close-type) and the dealt footer (footer-statement) on one screen. */
export default function Closing() {
  return (
    <footer className="footer closing footer--statement" id="contact" aria-labelledby="close-h">
      <div className="wrap">
        <div className="closing__cta close--type">
          <div className="section-head">
            <h2 id="close-h">Build SyscallGuard from source</h2>
            <p className="t-small muted">make, then make test. Needs gcc, make and python3. MIT licence.</p>
          </div>
          <a className="btn btn--primary btn--xl btn--wide" href={REPO}>
            {MAIN_ACTION}
            <Arrow />
          </a>
        </div>
        <div className="closing__links">
          <div className="grid">
            <div className="span-4 stack-4">
              <a className="nav__logo" href="#top">SyscallGuard</a>
              <p className="footer__lede">Cost budgets for Linux system calls.</p>
            </div>
            {COLUMNS.map((c) => (
              <div key={c.head} className="span-2 footer__col">
                <h3>{c.head}</h3>
                <ul>
                  {c.links.map(([href, label]) => (
                    <li key={label}><a href={href}>{label}</a></li>
                  ))}
                </ul>
              </div>
            ))}
          </div>
          <div className="footer__legal">
            <span>
              &copy; 2026 SyscallGuard &nbsp; <a href={`${REPO}/blob/main/LICENSE`}>MIT licence</a>
            </span>
            <div className="socials" />
          </div>
        </div>
      </div>
    </footer>
  );
}
