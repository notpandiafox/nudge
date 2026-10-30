// @vitest-environment jsdom
// The recorder's privacy filter (plan E2): what it keeps from NYT's page is
// what ends up committed as a test copy.
import { describe, expect, test } from 'vitest';

import { FAKE_ACCOUNTS, makeFixture, recordBoard, recordStorage } from '../recorder/fixture';

// Shaped like NYT's saved data (spike §3), with everything a copy must not keep.
function nytStorage(entries: Record<string, unknown>) {
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

const gameState = (hardMode: boolean) => ({
  states: [
    {
      puzzleId: '1594',
      data: {
        boardState: ['spike', 'grime', '', '', '', ''],
        evaluations: [['absent', 'absent', 'correct', 'absent', 'absent']],
        solution: 'brink',
        currentRowIndex: 2,
        status: 'IN_PROGRESS',
        hardMode,
        lastPlayedTs: 1761800000000,
      },
    },
  ],
  schemaVersion: '0.43.0',
});
const settings = (hardMode: boolean) => ({
  states: [{ puzzleId: 'settings', data: { hardMode, darkMode: true, colorblindMode: false } }],
  schemaVersion: '0.43.0',
});

describe('recordStorage', () => {
  const recorded = recordStorage(
    nytStorage({
      'games-state-wordleV2/103659666': gameState(true),
      'games-settings-wordleV2/103659666': settings(true),
      'games-state-wordleV2/ANON': gameState(false),
      'statsig.stable_id.1838556842': 'abc-def',
      'wordle-legacy-stats-103659666': { gamesPlayed: 40 },
      'geo-location': '{"country":"US"}',
    }),
  );

  test('keeps only the Wordle game and settings entries, with fake account numbers', () => {
    expect(Object.keys(recorded).sort()).toEqual([
      `games-settings-wordleV2/${FAKE_ACCOUNTS[0]}`,
      `games-state-wordleV2/${FAKE_ACCOUNTS[0]}`,
      'games-state-wordleV2/ANON',
    ]);
  });

  test('keeps the hard-mode facts and nothing else', () => {
    expect(recorded[`games-state-wordleV2/${FAKE_ACCOUNTS[0]}`]).toEqual({
      states: [
        { puzzleId: '1594', data: { currentRowIndex: 2, status: 'IN_PROGRESS', hardMode: true } },
      ],
      schemaVersion: '0.43.0',
    });
    expect(recorded[`games-settings-wordleV2/${FAKE_ACCOUNTS[0]}`]).toEqual({
      states: [{ puzzleId: 'settings', data: { hardMode: true } }],
      schemaVersion: '0.43.0',
    });
  });

  test('never keeps the guesses, the answer, or the real account number', () => {
    const text = JSON.stringify(recorded);
    for (const secret of ['spike', 'grime', 'brink', 'boardState', 'solution', '103659666']) {
      expect(text).not.toContain(secret);
    }
  });

  test('gives each real account its own fake number', () => {
    const two = recordStorage(
      nytStorage({
        'games-state-wordleV2/111111111': gameState(true),
        'games-state-wordleV2/222222222': gameState(false),
        'games-settings-wordleV2/111111111': settings(true),
      }),
    );
    expect(Object.keys(two).sort()).toEqual([
      `games-settings-wordleV2/${FAKE_ACCOUNTS[0]}`,
      `games-state-wordleV2/${FAKE_ACCOUNTS[0]}`,
      `games-state-wordleV2/${FAKE_ACCOUNTS[1]}`,
    ]);
  });

  test('records an entry it cannot parse as unreadable, not as its text', () => {
    const odd = recordStorage(nytStorage({ 'games-state-wordleV2/ANON': 'not json {spike' }));
    expect(odd).toEqual({ 'games-state-wordleV2/ANON': { unreadable: true } });
  });
});

describe('recordBoard', () => {
  const page = (body: string) => new DOMParser().parseFromString(body, 'text/html');

  test('saves the rows, and nothing around them', () => {
    const doc = page(`
      <header><span data-state="closed">Subscribe</span><a>oscar@example.com</a></header>
      <div id="board"><div aria-label="Row 1"><div data-state="absent">s</div></div>
      <div aria-label="Row 2"><div data-state="empty"></div></div></div>`);
    const html = recordBoard(doc);
    expect(html).toContain('aria-label="Row 1"');
    expect(html).toContain('aria-label="Row 2"');
    expect(html).not.toContain('Subscribe');
    expect(html).not.toContain('@');
  });

  test('saves only the rows even when nothing else separates them from the page', () => {
    // One row straight in <body>: "the smallest element holding every row"
    // would be the whole page, with whatever personal data it shows.
    const doc = page('<a>oscar@example.com</a><div aria-label="Row 1"><div>s</div></div>');
    expect(recordBoard(doc)).toBe('<div aria-label="Row 1"><div>s</div></div>');
  });

  test('saves nothing when the page has no rows', () => {
    expect(recordBoard(page('<p>Not Wordle</p>'))).toBeNull();
  });
});

describe('makeFixture', () => {
  test('dates the copy, keeps the note, and drops the address query', () => {
    const doc = new DOMParser().parseFromString('<div aria-label="Row 1"></div>', 'text/html');
    const fixture = makeFixture({
      document: doc,
      storage: nytStorage({}),
      href: 'https://www.nytimes.com/games/wordle/index.html?ref=someone',
      note: 'mid-game, hard mode on',
      now: new Date('2026-10-30T12:00:00Z'),
    });
    expect(fixture).toEqual({
      recordedOn: '2026-10-30',
      note: 'mid-game, hard mode on',
      page: 'https://www.nytimes.com/games/wordle/index.html',
      boardHtml: '<div aria-label="Row 1"></div>',
      storage: {},
    });
  });
});
