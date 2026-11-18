import { REPO } from './ui';

// Real marks of what the project runs on and with: the Linux kernel, Python tools, the Prometheus endpoint, the GitHub repo.
const ICONS = [
  { src: 'assets/icons/linux-tux.svg', alt: 'Linux' },
  { src: 'assets/icons/python.svg', alt: 'Python' },
  { src: 'assets/icons/prometheus.svg', alt: 'Prometheus' },
  { src: 'assets/icons/github.svg', alt: 'GitHub', ink: true },
];

const FAQ = [
  { q: 'Which kernels does it run on?', a: 'Linux 5.5 and up, 5.19 recommended, 6.6 for synchronous wake-up. x86-64 and aarch64.' },
  { q: 'Do I need root?', a: 'Not to enforce budgets. Only calibration needs root, for bpftrace.' },
  { q: 'Why does io_uring_setup fail?', a: 'It returns EPERM, because one io_uring_enter could submit many operations past per-call accounting.' },
  { q: 'What happens if sgd exits?', a: 'Costly calls of still-running jobs fail with ENOSYS. A filter cannot be removed from a running process.' },
  { q: 'Where should I try fork storms?', a: 'In a VM or a spare machine. Budgets slow a storm; sgctl kill ends it.' },
];

export default function Questions() {
  return (
    <section className="section" id="questions" aria-labelledby="questions-h">
      <div className="wrap">
        <div className="grid grid--top">
          <div className="span-5 stack-4">
            <h2 id="questions-h">Questions</h2>
            <p>The limits are in the README too. It builds with gcc and make; the tools need Python 3.</p>
            <div className="qs-icons">
              {ICONS.map((i) => (
                <span key={i.src} className="ic-tile ic-tile--48">
                  <img className={`ic ic-48${i.ink ? ' ic-ink' : ''}`} src={`${process.env.NEXT_PUBLIC_BASE_PATH ?? ''}/${i.src}`} alt={i.alt} width={48} height={48} />
                </span>
              ))}
            </div>
            <a className="link" href={`${REPO}#design-notes-and-limitations`}>Read the design notes</a>
          </div>
          <div className="faq span-6 start-7">
            {FAQ.map((f) => (
              <div key={f.q} className="faq__item">
                <h3>{f.q}</h3>
                <p>{f.a}</p>
              </div>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
}
