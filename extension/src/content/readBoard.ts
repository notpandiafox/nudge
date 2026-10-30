// Reads the Wordle board from NYT's page (spec, "Reading the board"; spike §2).
// Row by row, never by class names (they change with each NYT build), and
// every tile twice: its letter and colour, and its spoken label. Any
// disagreement means NYT's page has changed under us, so it reports
// "unreadable" rather than hint from a misread board.

// One finished row: the guess, and its colours as the solver takes them
// (G green, Y yellow, X gray).
export interface Row {
  word: string;
  pattern: string;
}

export type BoardReading = { readable: true; rows: Row[] } | { readable: false; reason: string };

// Also what the page looks like before NYT has drawn the board.
export const NO_ROWS = 'no "Row" elements on the page';

const COLOURS: Record<string, string> = { correct: 'G', present: 'Y', absent: 'X' };
// A tile with no colour yet: an empty row, or a guess still being typed.
const UNFINISHED = new Set(['empty', 'tbd']);
// The spoken label's words for each colour, e.g. "3rd letter, I, correct".
const SPOKEN: Record<string, string> = {
  G: 'correct',
  Y: 'present in another position',
  X: 'absent',
};

export function readBoard(document: Document): BoardReading {
  const rowElements = [...document.querySelectorAll('[aria-label^="Row "]')];
  if (rowElements.length === 0) return { readable: false, reason: NO_ROWS };

  const rows: Row[] = [];
  for (const rowElement of rowElements) {
    const name = rowElement.getAttribute('aria-label');
    const tiles = [...rowElement.querySelectorAll('[data-state]')];
    // Every row has five tiles, finished or not: anything else (including
    // none, if NYT renamed the attribute) means the page isn't what we know.
    if (tiles.length !== 5)
      return { readable: false, reason: `${name} has ${tiles.length} tiles, not 5` };

    const states = tiles.map((tile) => tile.getAttribute('data-state') ?? '');
    // Only these mean "not finished yet"; any other unknown value would skip
    // finished rows and read a mid-game board as empty.
    const unknown = states.findIndex((state) => !COLOURS[state] && !UNFINISHED.has(state));
    if (unknown !== -1) {
      return {
        readable: false,
        reason: `${name}, tile ${unknown + 1}: unknown colour "${states[unknown]}"`,
      };
    }
    const colours = states.map((state) => COLOURS[state]);
    if (!colours.every(Boolean)) continue; // empty, being typed, or still turning over

    let word = '';
    for (const [i, tile] of tiles.entries()) {
      const letter = tile.textContent?.trim() ?? '';
      const label = tile.getAttribute('aria-label') ?? '';
      const [, spokenLetter, spokenColour] = label.split(', ');
      if (
        !/^[a-z]$/.test(letter) ||
        spokenLetter !== letter.toUpperCase() ||
        spokenColour !== SPOKEN[colours[i]!]
      ) {
        return {
          readable: false,
          reason: `${name}, tile ${i + 1}: shows "${letter}" but its label says "${label}"`,
        };
      }
      word += letter;
    }
    rows.push({ word, pattern: colours.join('') });
  }
  return { readable: true, rows };
}
