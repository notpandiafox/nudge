// Builds the extension into dist/ (plan E-D5): bundles the TypeScript with
// esbuild and copies the manifest, the icon and the solver's WebAssembly build.
//
//   node build.mjs        (NUDGE_WASM_DIR overrides where the solver build is)
import { build } from 'esbuild';
import { access, copyFile, mkdir, rm } from 'node:fs/promises';

const solverDir = process.env.NUDGE_WASM_DIR ?? '../build-wasm/solver/wasm';
const solverFiles = ['nudge.mjs', 'nudge.wasm'];

for (const file of solverFiles) {
  await access(`${solverDir}/${file}`).catch(() => {
    throw new Error(
      `${solverDir}/${file} is missing: build the solver first (cmake --build build-wasm)`,
    );
  });
}

await rm('dist', { recursive: true, force: true });
await mkdir('dist/icons', { recursive: true });

const shared = { bundle: true, target: 'firefox128', logLevel: 'warning' };
// The background script is a module (it loads the solver with import()); a
// content script can't be one, so the page script is a classic script.
await build({
  ...shared,
  entryPoints: ['src/background/main.ts'],
  outfile: 'dist/background.js',
  format: 'esm',
});
await build({
  ...shared,
  entryPoints: ['src/content/main.ts'],
  outfile: 'dist/content.js',
  format: 'iife',
});

await copyFile('manifest.json', 'dist/manifest.json');
await copyFile('../assets/nudge_icon.svg', 'dist/icons/nudge.svg');
for (const file of solverFiles) {
  await copyFile(`${solverDir}/${file}`, `dist/${file}`);
}

// The recorder (plan E2), a separate developer extension that is never shipped.
await rm('dist-recorder', { recursive: true, force: true });
await mkdir('dist-recorder', { recursive: true });
for (const entry of ['content', 'popup']) {
  await build({
    ...shared,
    entryPoints: [`recorder/${entry}.ts`],
    outfile: `dist-recorder/${entry}.js`,
    format: 'iife',
  });
}
await copyFile('recorder/manifest.json', 'dist-recorder/manifest.json');
await copyFile('recorder/popup.html', 'dist-recorder/popup.html');
