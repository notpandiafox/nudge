// Node tests for the JavaScript interface (plan §5.15, T12), with Node's own
// test runner. The module comes from NUDGE_WASM, or from the default build:
//
//   NUDGE_WASM=build-wasm/solver/wasm/nudge.mjs node --test solver/wasm/tests/solver.test.mjs
import assert from 'node:assert/strict';
import { test } from 'node:test';
import { pathToFileURL } from 'node:url';

const modulePath = process.env.NUDGE_WASM
  ? pathToFileURL(process.env.NUDGE_WASM).href
  : new URL('../../../build-wasm/solver/wasm/nudge.mjs', import.meta.url).href;
let createModule;
try {
  createModule = (await import(modulePath)).default;
} catch (error) {
  throw new Error(`can't load ${modulePath}: build it (cmake --build build-wasm) or set NUDGE_WASM`,
                  { cause: error });
}
const nudge = await createModule();

const BUDGET_MS = 500;

// Every JavaScript-created solver must be deleted, or its C++ memory stays taken.
function withSolver(hardMode, use) {
  const solver = new nudge.Solver(hardMode);
  try {
    return use(solver);
  } finally {
    solver.delete();
  }
}

// The feedback rule, written again here so the game's colors don't come from
// the solver itself: Greens first, then Yellows left to right, using up copies.
function feedback(guess, answer) {
  const result = ['X', 'X', 'X', 'X', 'X'];
  const left = [...answer];
  for (let i = 0; i < 5; ++i) {
    if (guess[i] === answer[i]) {
      result[i] = 'G';
      left[i] = '#';
    }
  }
  for (let i = 0; i < 5; ++i) {
    const found = left.indexOf(guess[i]);
    if (result[i] !== 'G' && found !== -1) {
      result[i] = 'Y';
      left[found] = '#';
    }
  }
  return result.join('');
}

// Hard mode's rule B, also written again: every Green stays, and every lit
// letter of one earlier guess needs its own copy in the new guess.
function hardModeAllows(history, guess) {
  return history.every(([earlier, pattern]) => {
    const left = [...guess];
    for (let i = 0; i < 5; ++i) {
      if (pattern[i] === 'G' && guess[i] !== earlier[i]) return false;
      if (pattern[i] !== 'X') {
        const found = left.indexOf(earlier[i]);
        if (found === -1) return false;
        left[found] = '#';
      }
    }
    return true;
  });
}

// Plays one game against the answer; returns the guesses and colors, in order.
// fallbackSeen reports whether the solver was in fallback mode before the answer.
function play(solver, answer) {
  const history = [];
  let fallbackSeen = false;
  while (history.length < 10) {
    const guess = solver.getBestGuess();
    const pattern = feedback(guess, answer);
    assert.equal(solver.addFeedbackPattern(guess, pattern), 'Ok', `${guess} for ${answer}`);
    history.push([guess, pattern]);
    if (pattern === 'GGGGG') return { history, fallbackSeen };
    fallbackSeen ||= solver.isUsingFallback();
  }
  assert.fail(`${answer} not solved in 10 guesses`);
}

function replay(solver, history) {
  for (const [guess, pattern] of history) {
    assert.equal(solver.addFeedbackPattern(guess, pattern), 'Ok', `${guess} ${pattern}`);
  }
}

// The moves the native solver plays in these games (recorded with the native
// build, T12 review): the module the extension imports must play exactly the
// same. The full trees are compared by the benchmark's digests (plan §8.4).
// aahed was never a Wordle answer, so its games end in fallback.
const NATIVE_GAMES = {
  'minimax normal cigar': 'olate rynds circa cigar',
  'minimax normal hover': 'olate rocks nymph hover',
  'minimax normal poker': 'olate rocks joker poker',
  'minimax normal shave': 'olate crips dumky shave',
  'minimax normal mound': 'olate crony bumfs mound',
  'minimax normal catch': 'olate tacit bumph cawks catch',
  'minimax normal aahed': 'olate apgar aecia aahed',
  'minimax hard cigar': 'olate randy cigar',
  'minimax hard hover': 'olate rowme borek roven cover hover',
  'minimax hard poker': 'olate rowme borek joker poker',
  'minimax hard shave': 'olate scare shade shake shame shape shave',
  'minimax hard mound': 'olate crony bions pownd found hound mound',
  'minimax hard catch': 'olate tacit caput catch',
  'minimax hard aahed': 'olate asper abbey annex aided ached aahed',
  'entropy normal cigar': 'tarse brond mugil cigar',
  'entropy normal hover': 'tarse dinlo womby rover cover hover',
  'entropy normal poker': 'tarse dinlo womby rover joker poker',
  'entropy normal shave': 'tarse sulph dumky shave',
  'entropy normal mound': 'tarse colin bumph mound',
  'entropy normal catch': 'tarse clift catch',
  'entropy normal aahed': 'tarse lynch badam aahed',
  'entropy hard cigar': 'tarse brand cimar cigar',
  'entropy hard hover': 'tarse rider lower hover',
  'entropy hard poker': 'tarse rider lower hover yoker joker poker',
  'entropy hard shave': 'tarse spale shake shade shame shave',
  'entropy hard mound': 'tarse colin pownd bound found hound mound',
  'entropy hard catch': 'tarse hault batch match patch watch catch',
  'entropy hard aahed': 'tarse labne cagey mawed vaded faked hazed aahed',
};

test('builds no code at run time, which an extension forbids', async () => {
  // Firefox blocks new Function() and eval() in extension pages ("call to
  // Function() blocked by CSP", found by the extension spike). embind uses
  // new Function() unless built with -sDYNAMIC_EXECUTION=0.
  const { readFile } = await import('node:fs/promises');
  const source = await readFile(new URL(modulePath), 'utf8');
  assert.doesNotMatch(source, /new Function\b/);
  assert.doesNotMatch(source, /(^|[^\w.$])eval\(/);
});

test('starts the way the native solver does', () => {
  withSolver(false, (solver) => {
    assert.equal(solver.getRemainingCount(), 2309);
    assert.equal(solver.isUsingFallback(), false);
    assert.equal(solver.getBestGuess(), 'olate');
    assert.equal(solver.setStrategy('entropy'), 'Ok');
    assert.equal(solver.getBestGuess(), 'tarse');
  });
});

test('plays exactly the native games, in both modes and with both strategies', () => {
  for (const [game, nativeMoves] of Object.entries(NATIVE_GAMES)) {
    const [strategy, mode, answer] = game.split(' ');
    withSolver(mode === 'hard', (solver) => {
      assert.equal(solver.setStrategy(strategy), 'Ok');
      const { history, fallbackSeen } = play(solver, answer);
      assert.equal(history.map(([guess]) => guess).join(' '), nativeMoves, game);
      assert.equal(fallbackSeen, answer === 'aahed', `fallback in ${game}`);
      if (mode === 'hard') {
        history.forEach(([guess], i) =>
          assert.ok(hardModeAllows(history.slice(0, i), guess), `${guess} in ${game}`));
      }
    });
  }
});

test('every bad input comes back as a status text and changes nothing', () => {
  withSolver(false, (solver) => {
    const cases = [
      ['cr4ne', 'XXXXX', 'InvalidWord'],
      ['cranes', 'XXXXX', 'InvalidWord'],
      ['', 'XXXXX', 'InvalidWord'],
      ['cr4ne', 'nonsense', 'InvalidWord'], // the word is checked first, like natively
      ['crane', 'XXXX', 'InvalidPattern'],
      ['crane', 'XXXXQ', 'InvalidPattern'],
      ['crane', '', 'InvalidPattern'],
    ];
    for (const [word, pattern, status] of cases) {
      assert.equal(solver.addFeedbackPattern(word, pattern), status, `${word} ${pattern}`);
    }
    for (const name of ['knuth', 'Minimax ', 'ENTROPY', '']) {
      assert.equal(solver.setStrategy(name), 'UnknownStrategy', name);
    }
    assert.equal(solver.getRemainingCount(), 2309);
    assert.equal(solver.getBestGuess(), 'olate');

    // Upper and lower case both work; then a board no word can give.
    assert.equal(solver.addFeedbackPattern('CRANE', 'xxxxx'), 'Ok');
    const left = solver.getRemainingCount();
    assert.equal(solver.addFeedbackPattern('crane', 'YXXXX'), 'ImpossibleState');
    assert.equal(solver.getRemainingCount(), left);

    // After the answer, every further guess is refused.
    assert.equal(solver.addFeedbackPattern('pilot', 'GGGGG'), 'Ok');
    assert.equal(solver.addFeedbackPattern('slate', 'XXXXX'), 'GameOver');
    assert.equal(solver.getBestGuess(), 'pilot');
  });
});

test('wrong JavaScript types throw and leave the solver usable (plan §5.15)', () => {
  // Not a status: these are mistakes in the calling code, which embind rejects
  // before the solver sees them. The solver is unchanged afterwards.
  withSolver(false, (solver) => {
    const notStrings = [undefined, null, 12345, {}, ['c', 'r', 'a', 'n', 'e']];
    for (const value of notStrings) {
      assert.throws(() => solver.addFeedbackPattern(value, 'XXXXX'), nudge.BindingError, `${value}`);
      assert.throws(() => solver.addFeedbackPattern('crane', value), nudge.BindingError, `${value}`);
      assert.throws(() => solver.setStrategy(value), nudge.BindingError, `${value}`);
    }
    assert.throws(() => solver.addFeedbackPattern('crane'), nudge.BindingError);
    assert.throws(() => new nudge.Solver(), nudge.BindingError);
    assert.equal(solver.getRemainingCount(), 2309);
    assert.equal(solver.addFeedbackPattern('crane', 'XXXXX'), 'Ok');
    assert.ok(solver.getRemainingCount() < 2309);
  });
});

test('a deleted solver throws instead of reading freed memory', () => {
  const solver = new nudge.Solver(false);
  solver.delete();
  assert.throws(() => solver.getBestGuess(), nudge.BindingError);
  assert.throws(() => solver.addFeedbackPattern('crane', 'XXXXX'), nudge.BindingError);
  assert.throws(() => solver.delete(), nudge.BindingError);
});

test('a forgotten delete costs memory, not hints', () => {
  // The heap grows (plan §5.15), so leaked solvers don't run the module out of
  // memory: with the original fixed 16 MB, the 157th solver aborted.
  const leaked = [];
  try {
    for (let i = 0; i < 400; ++i) leaked.push(new nudge.Solver(false));
    withSolver(false, (solver) => {
      assert.equal(solver.addFeedbackPattern('crane', 'XXXXX'), 'Ok');
      assert.equal(typeof solver.getBestGuess(), 'string');
    });
  } finally {
    for (const solver of leaked) solver.delete();
  }
});

// The first position where normal and hard mode suggest different guesses, in
// the game against shave (one of hard mode's 7-guess answers).
function firstHardModeDifference() {
  const normal = withSolver(false, (solver) => play(solver, 'shave').history);
  const hard = withSolver(true, (solver) => play(solver, 'shave').history);
  const turn = hard.findIndex(([guess], i) => i >= normal.length || normal[i][0] !== guess);
  assert.ok(turn > 0 && turn < normal.length, 'normal and hard mode play the same game');
  return { history: hard.slice(0, turn), normalGuess: normal[turn][0], hardGuess: hard[turn][0] };
}

test('setHardMode switches the pool mid-game, and reset keeps the setting', () => {
  const { history, normalGuess, hardGuess } = firstHardModeDifference();
  withSolver(false, (solver) => {
    replay(solver, history);
    assert.equal(solver.getBestGuess(), normalGuess);
    solver.setHardMode(true);
    assert.equal(solver.getBestGuess(), hardGuess);

    solver.reset();
    assert.equal(solver.getRemainingCount(), 2309);
    replay(solver, history);
    assert.equal(solver.getBestGuess(), hardGuess); // still hard mode
  });
});

test('reset keeps the strategy', () => {
  withSolver(false, (solver) => {
    assert.equal(solver.setStrategy('entropy'), 'Ok');
    play(solver, 'cigar');
    solver.reset();
    assert.equal(solver.isUsingFallback(), false);
    assert.equal(solver.getBestGuess(), 'tarse');
  });
});

test('the worst fallback hint stays within twice the budget (CI gate, plan §8.5)', () => {
  // sughs with only its last letter green rules out every answer and leaves the
  // most guess-list words (plan §5.9). Best of 3, so one slow moment on a shared
  // machine doesn't decide it. Locally, read the time against 500 ms.
  for (const strategy of ['minimax', 'entropy']) {
    withSolver(false, (solver) => {
      assert.equal(solver.setStrategy(strategy), 'Ok');
      assert.equal(solver.addFeedbackPattern('sughs', 'XXXXG'), 'Ok');
      assert.equal(solver.isUsingFallback(), true);
      assert.equal(solver.getRemainingCount(), 2398);
      let best = Infinity;
      for (let run = 0; run < 3; ++run) {
        const start = performance.now();
        solver.getBestGuess();
        best = Math.min(best, performance.now() - start);
      }
      console.log(`worst fallback, ${strategy}, through the JavaScript interface: ${best.toFixed(1)} ms`);
      assert.ok(best < 2 * BUDGET_MS, `${best} ms`);
    });
  }
});
