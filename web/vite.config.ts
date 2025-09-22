/// <reference types="node" />
import { defineConfig } from 'vite';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import react from '@vitejs/plugin-react';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      fs: resolve(__dirname, 'src/shims/fs.ts'),
      path: resolve(__dirname, 'src/shims/path.ts'),
    },
  },
  define: {
    'process.env': {},
  },
});
