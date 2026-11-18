import { REPO } from '@/components/ui';

export default function NotFound() {
  return (
    <main id="top">
      <section className="section" aria-labelledby="nf-h">
        <div className="wrap stack-5">
          <h1 id="nf-h">No page at this address</h1>
          <p className="t-lead">This site is one page. The source and the docs are on GitHub.</p>
          <div className="btn-row">
            <a className="btn btn--primary" href="./">Go to the home page</a>
            <a className="btn btn--secondary" href={REPO}>Get the source</a>
          </div>
        </div>
      </section>
    </main>
  );
}
