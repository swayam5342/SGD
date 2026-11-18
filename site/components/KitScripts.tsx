'use client';

import { useEffect } from 'react';

// The kit's runtime: nav, fragments, loops, hovers, the hero's entrance, diagrams and the moment.
// Loaded once, after hydration; each script starts itself whether or not the document has finished loading.
const SCRIPTS = [
  'assets/system.js',
  'assets/motion/motion.js',
  'assets/layout/layout.js',
  'assets/diagrams/mount.js',
  'assets/moments/moments.js',
];

export default function KitScripts() {
  useEffect(() => {
    const added = SCRIPTS.filter((src) => !document.querySelector(`script[data-kit="${src}"]`)).map((src) => {
      const s = document.createElement('script');
      s.type = 'module';
      s.src = `${process.env.NEXT_PUBLIC_BASE_PATH ?? ''}/${src}`;
      s.dataset.kit = src;
      document.body.appendChild(s);
      return s;
    });
    return () => added.forEach((s) => s.remove());
  }, []);
  return null;
}
