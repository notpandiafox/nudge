// @vitest-environment jsdom
// The board reader (plan E3, spec "Reading the board") on the saved copies of
// NYT's page from E2.
import { readFileSync } from 'node:fs';
import { describe, expect, test } from 'vitest';

import { readBoard } from '../src/content/readBoard';

// A saved copy as a page: its rows, with NYT's menu buttons in front of them,
// which also carry data-state (the spike's first reader misread every row
// because of them).
function page(name: string, edit: (html: string) => string = (html) => html): Document {
  // Relative to extension/, where the tests run: under jsdom, import.meta.url
  // doesn't point at this file.
  const fixture = JSON.parse(readFileSync(`test/fixtures/wordle-2026-10-30-${name}.json`, 'utf8'));
  return new DOMParser().parseFromString(
    `<header><span data-state="closed"></span><span data-state="closed">Subscribe</span></header>
     <main>${edit(fixture.boardHtml)}</main>`,
    'text/html',
  );
}

describe('readBoard on the saved copies', () => {
  test('an empty board has no finished rows', () => {
    expect(readBoard(page('empty-hard-off'))).toEqual({ readable: true, rows: [] });
  });

  test('mid-game: the three finished rows, ignoring the empty ones', () => {
    expect(readBoard(page('mid-game'))).toEqual({
      readable: true,
      rows: [
        { word: 'adieu', pattern: 'XXXXX' },
        { word: 'strow', pattern: 'XYYGX' },
        { word: 'smart', pattern: 'XYXYY' },
      ],
    });
  });

  test('a won game, including a repeated letter with one copy gray (motts)', () => {
    const reading = readBoard(page('won'));
    expect(reading.readable && reading.rows.slice(3)).toEqual([
      { word: 'troop', pattern: 'YYYGX' },
      { word: 'motts', pattern: 'GGGXX' },
      { word: 'motor', pattern: 'GGGGG' },
    ]);
  });

  test('a lost game: six rows, none all green', () => {
    const reading = readBoard(page('lost-edited'));
    expect(reading.readable && reading.rows.map((row) => row.word)).toEqual([
      'adieu',
      'strow',
      'smart',
      'troop',
      'motts',
      'motto',
    ]);
  });
});

describe('readBoard says when it cannot read the board', () => {
  test("a tile's letter disagrees with its spoken label (the edited copy)", () => {
    expect(readBoard(page('unreadable-edited'))).toEqual({
      readable: false,
      reason: 'Row 2, tile 1: shows "x" but its label says "1st letter, S, absent"',
    });
  });

  test("a tile's colour disagrees with its spoken label", () => {
    const greenGray = (html: string) =>
      html.replace(
        'aria-label="4th letter, O, correct" data-state="correct"',
        'aria-label="4th letter, O, correct" data-state="absent"',
      );
    expect(readBoard(page('mid-game', greenGray))).toMatchObject({ readable: false });
  });

  test('a row without exactly five tiles', () => {
    const fourTiles = (html: string) =>
      html.replace(/<div class="Tile-module_tile__UWEHN"[^>]*>w<\/div>/, '');
    expect(readBoard(page('mid-game', fourTiles))).toEqual({
      readable: false,
      reason: 'Row 2 has 4 tiles, not 5',
    });
  });

  test('rows whose tiles have no colour attribute at all (NYT renamed it)', () => {
    // Without this, a renamed attribute would read as an empty board mid-game.
    const renamed = (html: string) => html.replaceAll('data-state=', 'data-tile-state=');
    expect(readBoard(page('mid-game', renamed))).toEqual({
      readable: false,
      reason: 'Row 1 has 0 tiles, not 5',
    });
  });

  test('a colour value it does not know (NYT renamed "correct")', () => {
    // Only "empty" and "tbd" mean "not finished yet"; anything else unknown
    // would otherwise skip every finished row and read as an empty board.
    const renamed = (html: string) => html.replaceAll('data-state="correct"', 'data-state="match"');
    expect(readBoard(page('mid-game', renamed))).toEqual({
      readable: false,
      reason: 'Row 2, tile 4: unknown colour "match"',
    });
  });

  test('no rows on the page', () => {
    const doc = new DOMParser().parseFromString(
      '<main><p>Wordle is loading</p></main>',
      'text/html',
    );
    expect(readBoard(doc)).toEqual({ readable: false, reason: 'no "Row" elements on the page' });
  });
});

describe('readBoard ignores rows that are not finished', () => {
  test('a row being typed, or turning over one tile at a time', () => {
    // Row 4 typed (tbd), then its first two tiles already turned over.
    const typing = (html: string) => {
      const rows = html.split('\n');
      rows[3] = rows[3]!
        .replace(/data-state="empty"/g, 'data-state="tbd"')
        .replace(
          'aria-label="1st letter, empty" data-state="tbd"',
          'aria-label="1st letter, C, absent" data-state="absent"',
        );
      return rows.join('\n');
    };
    const reading = readBoard(page('mid-game', typing));
    expect(reading.readable && reading.rows.length).toBe(3);
  });
});
