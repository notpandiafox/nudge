// Pre-commit hook (plan E-D12), installed by simple-git-hooks: checks the
// formatting and lint of the files being committed, so a mistake stops here
// instead of in CI. Git runs hooks from the repository root.
//
// shortcut: checks the files as they are on disk, not only their staged parts;
// a file that is partly staged is checked whole. Fine for one developer.
import { execFileSync, spawnSync } from 'node:child_process';

const staged = execFileSync('git', ['diff', '--cached', '--name-only', '--diff-filter=ACMR'], {
  encoding: 'utf8',
})
  .split('\n')
  .filter(Boolean);

const inExtension = staged
  .filter((file) => file.startsWith('extension/'))
  .map((file) => file.slice('extension/'.length));
const prettierFiles = inExtension.filter((file) => /\.(ts|mjs|json|md)$/.test(file));
const eslintFiles = inExtension.filter((file) => /\.(ts|mjs)$/.test(file));
const cppFiles = staged.filter((file) => /^solver\/.*\.(cpp|hpp)$/.test(file));

// No shell: arguments go to the program as they are, never through a command line.
let failed = false;
function run(command, args, cwd) {
  const result = spawnSync(command, args, { cwd, stdio: 'inherit' });
  failed ||= result.status !== 0;
}
// Prettier and ESLint through Node itself, so Windows needs no npx.cmd.
const node = (tool, args) => run(process.execPath, [`node_modules/${tool}`, ...args], 'extension');

if (prettierFiles.length > 0) node('prettier/bin/prettier.cjs', ['--check', ...prettierFiles]);
if (eslintFiles.length > 0) node('eslint/bin/eslint.js', eslintFiles);
if (cppFiles.length > 0) {
  // Only the version CI uses: another version can format differently and
  // block code CI accepts (CLAUDE.md).
  const version = spawnSync('clang-format', ['--version'], { encoding: 'utf8' });
  if (version.stdout?.includes('23.1.3')) {
    run('clang-format', ['--dry-run', '--Werror', ...cppFiles], '.');
  } else {
    console.warn(
      'pre-commit: clang-format 23.1.3 not found, so C++ formatting is checked only in CI.\n' +
        '            To check it here too: pip install clang-format==23.1.3',
    );
  }
}

if (failed) {
  console.error(
    'pre-commit: fix the problems above, then commit again ' +
      '(in extension/, "npm run format" fixes formatting).',
  );
  process.exit(1);
}
