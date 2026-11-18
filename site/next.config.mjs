// Static export for GitHub Pages. The site is served at the root of its custom domain (sdg.swayam.li),
// so there is no base path. Served from https://<user>.github.io/SGD/ without a custom domain instead,
// build with PAGES_BASE_PATH=/SGD.
const basePath = process.env.PAGES_BASE_PATH ?? '';

/** @type {import('next').NextConfig} */
const nextConfig = {
  output: 'export',
  trailingSlash: true,
  basePath,
  images: { unoptimized: true },
  // the same prefix for the plain links and scripts the kit needs (layout.tsx, KitScripts.tsx)
  env: { NEXT_PUBLIC_BASE_PATH: basePath },
};

export default nextConfig;
