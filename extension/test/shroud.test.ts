// The shroud (plan E6, spec "What the panel shows"), with moves from the
// native games in the solver's tests.
import { describe, expect, test } from 'vitest';

import { revealAll, revealLetter, shroud } from '../src/shroud';
import { feedback } from './reference';

const rowsFor = (answer: string, guesses: string[]) =>
  guesses.map((word) => ({ word, pattern: feedback(word, answer) }));

describe('shroud: which letters show from the start', () => {
  test('before any guess, every box is covered', () => {
    expect(shroud('olate', [])).toEqual({
      word: 'olate',
      shown: [false, false, false, false, false],
    });
  });

  test('a known green shows where the hint has the same letter (hard mode keeps them)', () => {
    // Minimax, hard mode, answer mound: crony makes the N green (4th); the
    // next hint, bions, has it there too.
    const rows = rowsFor('mound', ['olate', 'crony']);
    expect(rows[1]!.pattern).toBe('XXYGX');
    expect(shroud('bions', rows).shown).toEqual([false, false, false, true, false]);
  });

  test("a probe hint that doesn't keep a green shows nothing for it", () => {
    // Minimax, normal mode, answer catch: tacit makes the A green (2nd), and
    // the next hint, bumph, has a U there.
    const rows = rowsFor('catch', ['olate', 'tacit']);
    expect(rows[1]!.pattern).toBe('YGYXX');
    expect(shroud('bumph', rows).shown).toEqual([false, false, false, false, false]);
  });

  test('greens from different rows add up, yellows never show', () => {
    // Entropy, normal mode, answer hover: womby makes the O green (2nd), rover
    // makes O, V, E, R green; the next hint, cover, keeps all four.
    const rows = rowsFor('hover', ['tarse', 'dinlo', 'womby', 'rover']);
    expect(rows.map((row) => row.pattern)).toEqual(['XXYXY', 'XXXXY', 'XGXXX', 'XGGGG']);
    expect(shroud('cover', rows).shown).toEqual([false, true, true, true, true]);
  });
});

describe('revealLetter', () => {
  // A random source that gives the numbers in turn.
  const sequence =
    (...numbers: number[]) =>
    () =>
      numbers.shift() ?? 0;

  test('uncovers one covered box, chosen by the random number', () => {
    const start = shroud('bions', rowsFor('mound', ['olate', 'crony'])); // 4th shows
    // Covered boxes are 1st, 2nd, 3rd, 5th; 0.6 of 4 picks the third of them.
    expect(revealLetter(start, sequence(0.6)).shown).toEqual([false, false, true, true, false]);
  });

  test('pressed again and again, uncovers one new box each time until the word shows', () => {
    let current = shroud('olate', []);
    const random = sequence(0.99, 0, 0.5, 0.99, 0);
    const seen: number[] = [];
    for (let press = 1; press <= 5; ++press) {
      current = revealLetter(current, random);
      seen.push(current.shown.filter(Boolean).length);
    }
    expect(seen).toEqual([1, 2, 3, 4, 5]);
    expect(current.shown.every(Boolean)).toBe(true);
  });

  test('with nothing left to uncover, changes nothing', () => {
    const all = revealAll(shroud('olate', []));
    expect(revealLetter(all, sequence(0.5))).toEqual(all);
  });

  test('leaves the shroud it was given as it was', () => {
    const start = shroud('olate', []);
    revealLetter(start, sequence(0));
    expect(start.shown).toEqual([false, false, false, false, false]);
  });
});

describe('revealAll', () => {
  test('uncovers every box', () => {
    expect(revealAll(shroud('bumph', rowsFor('catch', ['olate', 'tacit'])))).toEqual({
      word: 'bumph',
      shown: [true, true, true, true, true],
    });
  });
});
