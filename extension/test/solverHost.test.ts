// The solver host (plan E5) with the real WebAssembly solver.
import { pathToFileURL } from 'node:url';
import { describe, expect, test } from 'vitest';

import { answer, type SolverModule } from '../src/background/solverHost';
import type { HintRequest, Strategy } from '../src/messages';
import { feedback, hardModeAllows } from './reference';

// The solver build the extension packages (build.mjs; CI downloads it there).
const solverDir = process.env.NUDGE_WASM_DIR ?? '../build-wasm/solver/wasm';
const createModule = (await import(pathToFileURL(`${solverDir}/nudge.mjs`).href)).default;
const nudge: SolverModule = await createModule();

const ask = (rows: HintRequest['rows'], hardMode = false, strategy: Strategy = 'minimax') =>
  answer(nudge, { type: 'hint', rows, hardMode, strategy });

// Plays a game the way the extension does: one request per turn, with every
// finished row so far. Returns the hints in order.
function play(target: string, hardMode: boolean, strategy: Strategy): string[] {
  const rows: HintRequest['rows'] = [];
  const hints: string[] = [];
  while (hints.length < 10) {
    const response = ask(rows, hardMode, strategy);
    if (response.kind !== 'hint')
      throw new Error(`turn ${rows.length + 1}: ${JSON.stringify(response)}`);
    hints.push(response.word);
    if (hardMode)
      expect(hardModeAllows(rows, response.word), `${response.word} in hard mode`).toBe(true);
    const pattern = feedback(response.word, target);
    rows.push({ word: response.word, pattern });
    if (pattern === 'GGGGG') return hints;
  }
  throw new Error(`${target} not solved`);
}

describe('answer', () => {
  // The moves the native solver plays (recorded for the solver's Node tests).
  test.each([
    ['minimax', false, 'cigar', 'olate rynds circa cigar'],
    ['entropy', false, 'hover', 'tarse dinlo womby rover cover hover'],
    ['minimax', true, 'mound', 'olate crony bions pownd found hound mound'],
    ['entropy', true, 'shave', 'tarse spale shake shade shame shave'],
  ] as const)(
    '%s, hard mode %s, playing %s, gives the native moves',
    (strategy, hardMode, target, moves) => {
      expect(play(target, hardMode, strategy).join(' ')).toBe(moves);
    },
  );

  test('a board no answer fits (fallback) has no word', () => {
    // sughs with only its last letter green rules out every answer.
    expect(ask([{ word: 'sughs', pattern: 'XXXXG' }])).toEqual({ kind: 'no-word' });
  });

  test('an impossible board has no word', () => {
    const rows = [
      { word: 'crane', pattern: 'XXXXX' },
      { word: 'crane', pattern: 'YXXXX' },
    ];
    expect(ask(rows)).toEqual({ kind: 'no-word' });
  });

  test('a won game is solved', () => {
    const rows = [
      { word: 'olate', pattern: 'XXXXX' },
      { word: 'cigar', pattern: 'GGGGG' },
    ];
    expect(ask(rows)).toEqual({ kind: 'solved', word: 'cigar' });
  });

  test('a row the reader should never send is an error, not a hint', () => {
    expect(ask([{ word: 'cr4ne', pattern: 'XXXXX' }])).toEqual({
      kind: 'error',
      message: 'cr4ne XXXXX: InvalidWord',
    });
    const unknown = answer(nudge, {
      type: 'hint',
      rows: [],
      hardMode: false,
      strategy: 'knuth' as Strategy,
    });
    expect(unknown).toEqual({ kind: 'error', message: 'strategy knuth: UnknownStrategy' });
  });
});

describe('answer always deletes its solver', () => {
  // A stand-in module that counts solvers, so a leak shows (plan E-D3).
  function counting(behaviour: { bestGuess: () => string }) {
    const live = { count: 0 };
    class Solver {
      constructor() {
        live.count += 1;
      }
      setStrategy = () => 'Ok';
      addFeedbackPattern = () => 'Ok';
      isUsingFallback = () => false;
      getBestGuess = behaviour.bestGuess;
      delete() {
        live.count -= 1;
      }
    }
    return { module: { Solver } as unknown as SolverModule, live };
  }

  test('after a hint', () => {
    const { module, live } = counting({ bestGuess: () => 'olate' });
    answer(module, { type: 'hint', rows: [], hardMode: false, strategy: 'minimax' });
    expect(live.count).toBe(0);
  });

  test('when the solver throws, and says so instead of throwing', () => {
    const { module, live } = counting({
      bestGuess: () => {
        throw new Error('boom');
      },
    });
    expect(
      answer(module, { type: 'hint', rows: [], hardMode: false, strategy: 'minimax' }),
    ).toEqual({
      kind: 'error',
      message: 'Error: boom',
    });
    expect(live.count).toBe(0);
  });
});
