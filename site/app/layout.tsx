import type { Metadata, Viewport } from 'next';
import type { ReactNode } from 'react';
import KitScripts from '@/components/KitScripts';

export const metadata: Metadata = {
  metadataBase: new URL('https://swayam5342.github.io/SGD/'),
  title: 'SyscallGuard: system-call budgets for Linux jobs',
  description:
    'Give every Linux job a cost budget for its system calls. A seccomp supervisor allows, holds or rejects each costly call, with no kernel module.',
  openGraph: {
    type: 'website',
    title: 'SyscallGuard: system-call budgets for Linux jobs',
    description: 'A seccomp supervisor that allows, holds or rejects each costly system call against a per-job budget.',
    images: ['share.jpg'],
  },
  twitter: { card: 'summary_large_image' },
  alternates: { canonical: './' },
  other: { 'site-kind': 'software', 'page-type': 'software' },
};

export const viewport: Viewport = {
  width: 'device-width',
  initialScale: 1,
  viewportFit: 'cover',
  themeColor: '#161c1a',
};

// The design kit in public/assets is plain CSS and JS. Its stylesheets load here in the kit's order;
// its scripts load after React hydrates (KitScripts), so React never fights the DOM the kit builds.
// Paths carry the GitHub Pages base path (/SGD) so they also resolve from the 404 page at any depth.
const BASE = process.env.NEXT_PUBLIC_BASE_PATH ?? '';
const KIT_CSS = [
  'assets/system.css',
  'assets/ui/cards.css',
  'assets/motion/motion.css',
  'assets/backgrounds/backgrounds.css',
  'assets/layout/layout.css',
  'assets/icons/icon.css',
  'tokens.css',
  'assets/ui/base.css',
  'assets/ui/window/fragment.css',
  'assets/ui/terminal-run/fragment.css',
  'assets/ui/toggles/fragment.css',
  'assets/ui/notify/fragment.css',
  'assets/ui/code/fragment.css',
  'assets/diagrams/diagram.css',
  'assets/moments/liquid-type/moment.css',
  'page.css',
];

const BOOT = [
  "document.documentElement.classList.add('m-enter');setTimeout(()=>document.documentElement.classList.add('m-enter-done'),700);",
  "document.documentElement.classList.add('mo-js');setTimeout(()=>document.documentElement.classList.add('mo-safe'),2500);",
].join('');

export default function RootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en" data-type="software" data-radius="sharp" data-ground="sand" data-buttons="solid" data-motion="" suppressHydrationWarning>
      <head>
        <link rel="preconnect" href="https://fonts.googleapis.com" />
        <link rel="preconnect" href="https://fonts.gstatic.com" crossOrigin="" />
        <link
          rel="stylesheet"
          href="https://fonts.googleapis.com/css2?family=Funnel+Display:wght@400..700&family=Funnel+Sans:wght@400..600&family=DM+Mono:wght@400;500&display=swap"
        />
        {KIT_CSS.map((href) => (
          <link key={href} rel="stylesheet" href={`${BASE}/${href}`} />
        ))}
        <script dangerouslySetInnerHTML={{ __html: BOOT }} />
        {/* eslint-disable-next-line @next/next/no-sync-scripts */}
        <script src={`${BASE}/assets/backgrounds/backgrounds.js`} />
      </head>
      <body suppressHydrationWarning>
        {children}
        <KitScripts />
      </body>
    </html>
  );
}
