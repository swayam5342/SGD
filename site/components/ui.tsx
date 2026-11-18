import type { CSSProperties } from 'react';

export const REPO = 'https://github.com/swayam5342/SGD';
export const MAIN_ACTION = 'Get the source';

/** Inline style with CSS custom properties (--ax, --fit-max ...), which CSSProperties does not type. */
export const vars = (v: Record<string, string>) => v as CSSProperties;

export function Arrow() {
  return (
    <svg className="m-arrow" viewBox="0 0 12 12" aria-hidden="true">
      <path className="m-arrow__stem" d="M1 6h8" />
      <path className="m-arrow__head" d="M4.5 2.5 8 6l-3.5 3.5" />
    </svg>
  );
}

export function Chevron() {
  return (
    <svg className="chev" viewBox="0 0 12 12" aria-hidden="true">
      <path d="M3 4.5l3 3 3-3" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  );
}

export function Tick({ className = 'ui-icon' }: { className?: string }) {
  return (
    <svg className={className} viewBox="0 0 16 16" aria-hidden="true">
      <path d="m3.5 8.4 2.8 2.8 6.2-6.4" />
    </svg>
  );
}

export function Dots() {
  return (
    <span className="ui-dots" aria-hidden="true">
      <i />
      <i />
      <i />
    </span>
  );
}
