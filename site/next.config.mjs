// Static export for GitHub Pages. The repo is served at https://<user>.github.io/SGD/,
// so the Pages build (GITHUB_PAGES=true, set in the workflow) prefixes every route with /SGD.
const isPages = process.env.GITHUB_PAGES === 'true';

/** @type {import('next').NextConfig} */
const nextConfig = {
  output: 'export',
  trailingSlash: true,
  basePath: isPages ? '/SGD' : '',
  images: { unoptimized: true },
  // the same prefix for the plain links and scripts the kit needs (layout.tsx, KitScripts.tsx)
  env: { NEXT_PUBLIC_BASE_PATH: isPages ? '/SGD' : '' },
};

export default nextConfig;
