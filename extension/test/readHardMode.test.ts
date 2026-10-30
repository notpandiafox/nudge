// The hard-mode reader (plan E4, spec "Reading hard mode") on the saved data
// from the E2 copies. Their real case: the signed-in entry holds an old game
// (5 rows, hard mode off) while today's game is in the signed-out (ANON) one.
import { readFileSync } from 'node:fs';
import { describe, expect, test } from 'vitest';

import { readHardMode } from '../src/content/readHardMode';

type Entries = Record<string, unknown>;

// Saved data the way the page has it: each entry is text (JSON).
function storage(entries: Entries) {
  const keys = Object.keys(entries);
  return {
    get length() {
      return keys.length;
    },
    key: (index: number) => keys[index] ?? null,
    getItem: (key: string) => {
      const value = entries[key];
      return value === undefined ? null : typeof value === 'string' ? value : JSON.stringify(value);
    },
  };
}

function saved(name: string): Entries {
  return JSON.parse(readFileSync(`test/fixtures/wordle-2026-10-30-${name}.json`, 'utf8')).storage;
}

const game = (rows: number, hardMode: unknown) => ({
  states: [{ puzzleId: '3616', data: { currentRowIndex: rows, status: 'IN_PROGRESS', hardMode } }],
});
const settings = (hardMode: unknown) => ({
  states: [{ puzzleId: 'settings', data: { hardMode } }],
});

describe('readHardMode on the saved copies', () => {
  test("today's game decides, not the old signed-in game or the disagreeing settings", () => {
    // empty-hard-on: ANON game 0 rows, hard mode on; account game 5 rows, off;
    // settings: account off, ANON on.
    expect(readHardMode(storage(saved('empty-hard-on')), 0)).toEqual({
      readable: true,
      hardMode: true,
    });
  });

  test.each([
    ['empty-hard-off', 0],
    ['mid-game', 3],
    ['won', 6],
    ['lost-edited', 6],
  ])('%s with %i finished rows: hard mode off', (name, rows) => {
    expect(readHardMode(storage(saved(name)), rows)).toEqual({ readable: true, hardMode: false });
  });

  test('matching is by finished rows: 5 rows picks the old signed-in game', () => {
    expect(readHardMode(storage(saved('empty-hard-on')), 5)).toEqual({
      readable: true,
      hardMode: false,
    });
  });
});

test("today's game comes before the settings when they differ", () => {
  // A game started in hard mode, with hard mode switched off since (for the
  // next game): this game is still played in hard mode.
  const entries = {
    'games-state-wordleV2/ANON': game(3, true),
    'games-settings-wordleV2/ANON': settings(false),
  };
  expect(readHardMode(storage(entries), 3)).toEqual({ readable: true, hardMode: true });
});

describe('readHardMode without a matching game', () => {
  test('uses the settings when they agree', () => {
    const entries = {
      'games-state-wordleV2/ANON': game(4, false),
      'games-settings-wordleV2/ANON': settings(true),
    };
    expect(readHardMode(storage(entries), 1)).toEqual({ readable: true, hardMode: true });
  });

  test('is unreadable when the settings disagree (signed in and signed out)', () => {
    // empty-hard-on's games have 0 and 5 rows, so 2 rows matches neither.
    expect(readHardMode(storage(saved('empty-hard-on')), 2)).toEqual({
      readable: false,
      reason: 'no saved game matches the 2 finished rows, and the saved settings disagree',
    });
  });

  test('is unreadable when two matching games disagree and there are no settings', () => {
    const entries = {
      'games-state-wordleV2/ANON': game(2, true),
      'games-state-wordleV2/10000001': game(2, false),
    };
    expect(readHardMode(storage(entries), 2)).toMatchObject({ readable: false });
  });

  test('says there are no settings when there are none, rather than that they disagree', () => {
    const entries = { 'games-state-wordleV2/ANON': game(4, false) };
    expect(readHardMode(storage(entries), 1)).toEqual({
      readable: false,
      reason: 'no saved game matches the 1 finished rows, and there are no saved settings',
    });
  });

  test('is unreadable when NYT keeps no Wordle data at all', () => {
    expect(readHardMode(storage({ 'nyt-wordle-darkmode': 'true' }), 0)).toEqual({
      readable: false,
      reason: 'no saved Wordle game or settings',
    });
  });

  test('ignores entries it cannot read, instead of failing on them', () => {
    const entries = {
      'games-state-wordleV2/ANON': 'not json {',
      'games-state-wordleV2/10000001': game(1, 'yes'), // hardMode not true or false
      'games-settings-wordleV2/ANON': { states: 'nope' },
      'games-settings-wordleV2/10000001': settings(false),
    };
    expect(readHardMode(storage(entries), 1)).toEqual({ readable: true, hardMode: false });
  });
});
