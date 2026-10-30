// The background script: hosts the solver (plan E-D2, E-D3). The page script
// asks it for hints; it answers with solverHost.
import type { HintRequest, HintResponse } from '../messages';
import { answer, type SolverModule } from './solverHost';

// Loaded once, on the first request, from the extension's own copy of the
// solver (built into dist/ by build.mjs). By its URL, not an import, so the
// bundle doesn't try to include it. A failed load is forgotten, so the next
// request tries again instead of failing for good.
let solver: Promise<SolverModule> | undefined;
function loadSolver(): Promise<SolverModule> {
  solver ??= import(browser.runtime.getURL('nudge.mjs'))
    .then((module: { default: () => Promise<SolverModule> }) => module.default())
    .catch((error: unknown) => {
      solver = undefined;
      throw error;
    });
  return solver;
}

browser.runtime.onMessage.addListener((message: Partial<HintRequest> | undefined) => {
  if (message?.type !== 'hint') return undefined;
  // Every outcome is one of the four answers, a failed load included.
  return loadSolver().then(
    (module): HintResponse => answer(module, message as HintRequest),
    (error: unknown): HintResponse => ({ kind: 'error', message: `loading the solver: ${error}` }),
  );
});
