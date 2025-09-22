#!/usr/bin/env node
const fs = require('fs/promises');
const path = require('path');

const root = process.cwd();
const sourceDir = path.join(root, 'data');
const distDir = path.join(root, 'dist', 'data');

async function copyRecursive() {
  try {
    await fs.mkdir(distDir, { recursive: true });
    const entries = await fs.readdir(sourceDir);
    await Promise.all(
      entries.map(async (entry) => {
        const src = path.join(sourceDir, entry);
        const dest = path.join(distDir, entry);
        await fs.copyFile(src, dest);
      }),
    );
    console.log(`Copied data assets to ${distDir}`);
  } catch (err) {
    if (err && err.code === 'ENOENT') {
      return;
    }
    console.error('Failed to copy data assets', err);
    process.exit(1);
  }
}

copyRecursive();
