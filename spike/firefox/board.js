// Spike content script: runs on the Wordle page and reports what it can see.
// It only reads: the board's tiles and the page's own saved data.

const COLORS = { correct: 'G', present: 'Y', absent: 'X' };

function readBoard() {
  const tiles = [...document.querySelectorAll('[data-state]')];
  // The raw facts, so we learn the real structure instead of guessing it.
  const rawTiles = tiles.slice(0, 12).map((tile) => ({
    tag: tile.tagName.toLowerCase(),
    className: String(tile.className).slice(0, 80),
    dataState: tile.getAttribute('data-state'),
    ariaLabel: tile.getAttribute('aria-label'),
    text: tile.textContent,
    rowLabel: tile.closest('[aria-label^="Row"]')?.getAttribute('aria-label') ?? null,
  }));

  // The finished rows, read row by row: each "Row N" element and the tiles
  // inside it (the first try grouped every [data-state] on the page in fives,
  // and NYT's menu buttons, which come first, shifted every row by three).
  // Each tile's letter and color are checked against its spoken label, e.g.
  // "3rd letter, I, correct", so a misread shows up as a mismatch.
  const rows = [];
  const mismatches = [];
  const LABEL_COLORS = { correct: 'G', 'present in another position': 'Y', absent: 'X' };
  for (const rowElement of document.querySelectorAll('[aria-label^="Row "]')) {
    const rowTiles = [...rowElement.querySelectorAll('[data-state]')];
    if (rowTiles.length !== 5 || !rowTiles.every((tile) => COLORS[tile.getAttribute('data-state')])) {
      continue; // empty or still being typed
    }
    const word = rowTiles.map((tile) => tile.textContent.trim().toLowerCase()).join('');
    const pattern = rowTiles.map((tile) => COLORS[tile.getAttribute('data-state')]).join('');
    rowTiles.forEach((tile, i) => {
      const [, letter, said] = (tile.getAttribute('aria-label') ?? '').split(', ');
      if (letter?.toLowerCase() !== word[i] || LABEL_COLORS[said] !== pattern[i]) {
        mismatches.push(`${rowElement.getAttribute('aria-label')} tile ${i + 1}: ${tile.getAttribute('aria-label')}`);
      }
    });
    rows.push({ row: rowElement.getAttribute('aria-label'), word, pattern });
  }

  // Where might hard mode be? Saved data first (only short pieces around
  // "hard", so nothing else of the page's storage is shown), then the page.
  const storage = [];
  for (let i = 0; i < localStorage.length; ++i) {
    const key = localStorage.key(i);
    const value = localStorage.getItem(key) ?? '';
    const at = value.toLowerCase().indexOf('hard');
    storage.push({
      key,
      length: value.length,
      aroundHard: at === -1 ? null : value.slice(Math.max(0, at - 40), at + 60),
    });
  }
  const hardModeOnPage = [...document.querySelectorAll('[aria-label*="ard" i], [id*="hard" i]')]
    .slice(0, 5)
    .map((element) => ({
      tag: element.tagName.toLowerCase(),
      id: element.id,
      ariaLabel: element.getAttribute('aria-label'),
      checked: element.getAttribute('aria-checked') ?? element.checked ?? null,
    }));

  return { url: location.href, tileCount: tiles.length, rows, mismatches, storage, hardModeOnPage, rawTiles };
}

browser.runtime.onMessage.addListener((message) =>
  message?.type === 'read-board' ? Promise.resolve(readBoard()) : undefined);
