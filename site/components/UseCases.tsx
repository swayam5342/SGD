// The three cases are the README's own Usage examples; each command is quoted as the README writes it.
const CASES = [
  {
    title: 'Keeps a build fast next to a noisy logger',
    body: 'At priority 2 the logger gets the smaller share, so its costly calls wait while the build at priority 8 keeps going.',
    cmds: ['./build/sg-launch -p 8 -n build -- make -j4', './build/sg-launch -p 2 -n logger -- ./noisy-logger'],
  },
  {
    title: 'Holds a batch job to a fixed rate',
    body: 'A batch job gets a fixed 500 cost units a second instead of a share that changes with the other jobs.',
    cmds: ['./build/sg-launch -r 500 -- ./batch'],
  },
  {
    title: 'Contains a script you do not trust',
    body: 'Over budget, its attempts to start new processes fail at once. sgctl kill stops its whole process group.',
    cmds: ['./build/sg-launch -f reject -- ./untrusted', './build/sgctl kill 4'],
  },
];

/** What the tool is for, in the visitor's terms, before how it works. */
export default function UseCases() {
  return (
    <section className="section" id="uses" aria-labelledby="uses-h">
      <div className="wrap">
        <div className="section-head">
          <h2 id="uses-h">
            Three uses for SyscallGuard
          </h2>
          <p className="t-lead">
            Some system calls are expensive: flushing files to disk, creating files, starting processes. A job that makes thousands of them slows down everything else on the same Linux machine. SyscallGuard gives each job a budget for those calls, so the busy job slows down instead.
          </p>
        </div>
        <div className="grid">
          {CASES.map((c) => (
            <article key={c.title} className="card card--row card--open span-4">
              <h3>{c.title}</h3>
              <p>{c.body}</p>
              <div className="panel panel--flush">
                <div className="pc-term pc-term--pane" role="img" aria-label={c.cmds.join('; ')}>
                  {c.cmds.map((l) => (
                    <span key={l} className="cmd">$ <b>{l}</b></span>
                  ))}
                </div>
              </div>
            </article>
          ))}
        </div>
      </div>
    </section>
  );
}
