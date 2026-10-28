import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    include: ['test/**/*.test.ts'],
    coverage: {
      provider: 'v8',
      // Every source file counts, including ones no test imports (plan E-D7).
      include: ['src/**/*.ts'],
      thresholds: { lines: 100, functions: 100 },
      reporter: ['text', 'html'],
    },
  },
});
