// Answers one hint request with the solver (plan E5, E-D3): a fresh Solver
// for every request, given every finished row on screen, deleted afterwards.
// So there's no game state to keep in step with the page, nothing to lose
// when Firefox suspends the background script, and no solver left behind.
import type { HintRequest, HintResponse } from '../messages';

// The part of the solver's JavaScript interface used here (solver plan §5.15).
interface Solver {
  setStrategy(name: string): string;
  addFeedbackPattern(word: string, pattern: string): string;
  getBestGuess(): string;
  isUsingFallback(): boolean;
  delete(): void;
}

export interface SolverModule {
  Solver: new (hardMode: boolean) => Solver;
}

export function answer(module: SolverModule, request: HintRequest): HintResponse {
  const solver = new module.Solver(request.hardMode);
  try {
    const strategy = solver.setStrategy(request.strategy);
    if (strategy !== 'Ok')
      return { kind: 'error', message: `strategy ${request.strategy}: ${strategy}` };
    for (const { word, pattern } of request.rows) {
      const status = solver.addFeedbackPattern(word, pattern);
      if (status === 'ImpossibleState') return { kind: 'no-word' };
      if (status !== 'Ok') return { kind: 'error', message: `${word} ${pattern}: ${status}` };
    }
    const last = request.rows.at(-1);
    if (last?.pattern === 'GGGGG') return { kind: 'solved', word: last.word };
    if (solver.isUsingFallback()) return { kind: 'no-word' };
    return { kind: 'hint', word: solver.getBestGuess() };
  } catch (error) {
    return { kind: 'error', message: String(error) };
  } finally {
    solver.delete();
  }
}
