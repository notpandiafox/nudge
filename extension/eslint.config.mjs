import js from '@eslint/js';
import globals from 'globals';
import tseslint from 'typescript-eslint';

export default tseslint.config(
  { ignores: ['dist/', 'dist-recorder/', 'coverage/', 'web-ext-artifacts/', 'node_modules/'] },
  js.configs.recommended,
  ...tseslint.configs.recommended,
  // The build and hook scripts run in Node.
  { files: ['*.mjs', 'scripts/**/*.mjs'], languageOptions: { globals: globals.node } },
);
