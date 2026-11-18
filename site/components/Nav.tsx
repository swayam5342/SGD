import { MAIN_ACTION, REPO } from './ui';

/** The bar holds only the name and the one action: a one-page site needs no link list. */
export default function Nav() {
  return (
    <header className="nav">
      <div className="wrap nav__bar">
        <a className="skip-link" href="#top">Skip to content</a>
        <a className="nav__logo" href="#top">SyscallGuard</a>
        <div className="nav__actions">
          <a className="btn btn--primary" href={REPO}>{MAIN_ACTION}</a>
        </div>
      </div>
    </header>
  );
}
