// @vitest-environment jsdom
// The page script (plan E7, spec "When the panel appears"): the saved copies,
// the real panel, and the real solver standing in for the background script.
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { resolve } from 'node:path';
import { afterEach, describe, expect, test, vi } from 'vitest';

import { answer, type SolverModule } from '../src/background/solverHost';
import type { HintRequest, HintResponse } from '../src/messages';
import { watchPage, type Page } from '../src/content/page';
import { createPanel } from '../src/content/panel';
import { DEFAULTS, type Settings } from '../src/settings';
import { shroud } from '../src/shroud';

const solverDir = process.env.NUDGE_WASM_DIR ?? '../build-wasm/solver/wasm';
// Loaded by Node itself: in the jsdom environment Vitest's import can't reach
// files outside extension/.
const solverPath = resolve(solverDir, 'nudge.mjs');
const nudge: SolverModule = await createRequire(solverPath)(solverPath).default();

function saved(name: string) {
  return JSON.parse(readFileSync(`test/fixtures/wordle-2026-10-30-${name}.json`, 'utf8'));
}

function storageOf(entries: Record<string, unknown>) {
  const keys = Object.keys(entries);
  return {
    length: keys.length,
    key: (index: number) => keys[index] ?? null,
    getItem: (key: string) => (key in entries ? JSON.stringify(entries[key]) : null),
  };
}

let running: Page | undefined;
afterEach(() => running?.stop());

// Opens a saved copy and starts nudge on it, with the real solver answering.
function open(
  name: string,
  ask?: (request: HintRequest) => Promise<HintResponse>,
  storage = saved(name).storage,
  settings: Settings = DEFAULTS,
) {
  const fixture = saved(name);
  document.body.innerHTML = `<main>${fixture.boardHtml}</main>`;
  const requests: HintRequest[] = [];
  const panel = createPanel(document);
  running = watchPage({
    document,
    storage: storageOf(storage),
    panel,
    settings,
    ask: (request) => {
      requests.push(request);
      return ask ? ask(request) : Promise.resolve(answer(nudge, request));
    },
  });
  const shadow = () => panel.host.shadowRoot!;
  return {
    requests,
    panel,
    watcher: running,
    boxes: () => [...shadow().querySelectorAll('.box')].map((box) => box.textContent),
    text: () => shadow().querySelector('.body')!.textContent,
    close: () => shadow().querySelector<HTMLButtonElement>('[aria-label="Close"]')!.click(),
  };
}

// The won game is the mid-game one played on: troop, motts, then motor.
const [, , , troop, , motor] = saved('won').boardHtml.split('\n') as string[];

// Plays a guess into row 4 the way NYT does: its tiles turn over in place.
function guess(rowHtml: string) {
  const played = document.createElement('template');
  played.innerHTML = rowHtml;
  const from = played.content.querySelectorAll('[data-state]');
  document.querySelectorAll('[aria-label="Row 4"] [data-state]').forEach((tile, i) => {
    tile.textContent = from[i]!.textContent;
    tile.setAttribute('aria-label', from[i]!.getAttribute('aria-label')!);
    tile.setAttribute('data-state', from[i]!.getAttribute('data-state')!);
  });
}

describe('when the page opens', () => {
  test('an empty board: the opener, covered', async () => {
    const page = open('empty-hard-off');
    await vi.waitFor(() => expect(page.boxes()).toEqual(['', '', '', '', '']));
    expect(page.requests).toEqual([
      { type: 'hint', rows: [], hardMode: false, strategy: 'minimax' },
    ]);
    expect(page.panel.host.hidden).toBe(false);
  });

  test("the player's hard mode goes with the request", async () => {
    const page = open('empty-hard-on');
    await vi.waitFor(() => expect(page.requests).toHaveLength(1));
    expect(page.requests[0]!.hardMode).toBe(true);
  });

  test('mid-game: the solver’s next guess for the three rows on screen', async () => {
    const page = open('mid-game');
    await vi.waitFor(() => expect(page.boxes()).toHaveLength(5));
    expect(page.requests[0]!.rows.map((row) => row.word)).toEqual(['adieu', 'strow', 'smart']);
    const hint = answer(nudge, page.requests[0]!);
    if (hint.kind !== 'hint') throw new Error(`expected a hint, got ${hint.kind}`);
    const { word, shown } = shroud(hint.word, page.requests[0]!.rows);
    expect(page.boxes()).toEqual(shown.map((isShown, i) => (isShown ? word[i] : '')));
  });

  test.each(['won', 'lost-edited'])('a finished game (%s): quiet, nothing asked', async (name) => {
    const page = open(name);
    await new Promise((settle) => setTimeout(settle, 20));
    expect(page.requests).toEqual([]);
    expect(page.panel.host.hidden).toBe(true);
  });

  test('an unreadable board: says so, asks for a report, asks the solver nothing', async () => {
    const page = open('unreadable-edited');
    await vi.waitFor(() => expect(page.text()).toContain("nudge can't read the board"));
    expect(page.text()).toContain('Row 2, tile 1');
    expect(page.requests).toEqual([]);
  });
});

describe('after each guess', () => {
  test('opens again, closed or not, with a hint for the new board', async () => {
    const page = open('mid-game');
    await vi.waitFor(() => expect(page.requests).toHaveLength(1));
    page.close();
    guess(troop!);
    await vi.waitFor(() => expect(page.requests).toHaveLength(2));
    expect(page.requests[1]!.rows.map((row) => row.word)).toEqual([
      'adieu',
      'strow',
      'smart',
      'troop',
    ]);
    await vi.waitFor(() => expect(page.panel.host.hidden).toBe(false));
  });

  test('a winning guess closes the panel and asks nothing more', async () => {
    const page = open('mid-game');
    await vi.waitFor(() => expect(page.panel.host.hidden).toBe(false));
    guess(motor!);
    await vi.waitFor(() => expect(page.panel.host.hidden).toBe(true));
    expect(page.requests).toHaveLength(1);
  });

  test('typing a guess, or a tile turning over, asks nothing new', async () => {
    const page = open('mid-game');
    await vi.waitFor(() => expect(page.requests).toHaveLength(1));
    const tile = document
      .querySelectorAll('[aria-label^="Row "]')[3]!
      .querySelector('[data-state]')!;
    tile.setAttribute('data-state', 'tbd');
    tile.textContent = 'c';
    await new Promise((settle) => setTimeout(settle, 20));
    expect(page.requests).toHaveLength(1);
  });

  test('a late answer for an older board never replaces the newer one', async () => {
    const pending: ((response: HintResponse) => void)[] = [];
    const page = open('mid-game', () => new Promise((settle) => pending.push(settle)));
    await vi.waitFor(() => expect(pending).toHaveLength(1));
    guess(troop!);
    await vi.waitFor(() => expect(pending).toHaveLength(2));
    pending[1]!({ kind: 'hint', word: 'floor' }); // the newer board answers first
    pending[0]!({ kind: 'hint', word: 'stale' });
    await new Promise((settle) => setTimeout(settle, 20));
    // floor keeps the green O (4th) of strow and troop; stale would cover all five.
    expect(page.boxes()).toEqual(['', '', '', 'o', '']);
  });
});

describe('the other answers', () => {
  test.each([
    [{ kind: 'no-word' } as HintResponse, "we don't know what the word is :C"],
    [
      { kind: 'error', message: 'cr4ne XXXXX: InvalidWord' } as HintResponse,
      'nudge ran into a problem.Please report it on GitHub.cr4ne XXXXX: InvalidWord',
    ],
  ])('%j shows its message', async (response, message) => {
    const page = open('mid-game', () => Promise.resolve(response));
    await vi.waitFor(() => expect(page.text()).toBe(message));
  });

  test('a solved board needs no panel', async () => {
    const page = open('mid-game', () => Promise.resolve({ kind: 'solved', word: 'smart' }));
    await vi.waitFor(() => expect(page.requests).toHaveLength(1));
    await new Promise((settle) => setTimeout(settle, 20));
    expect(page.panel.host.hidden).toBe(true);
    page.panel.toggle();
    expect(page.text()).toBe('This game is over.');
  });

  test('unreadable hard mode: says so, asks the solver nothing', async () => {
    const page = open('mid-game', undefined, {});
    await vi.waitFor(() => expect(page.text()).toContain('no saved Wordle game or settings'));
    expect(page.text()).toContain("nudge can't read the board");
    expect(page.requests).toEqual([]);
  });

  test('a background that cannot be reached is a problem, not silence', async () => {
    const page = open('mid-game', () =>
      Promise.reject(new Error('Could not establish connection')),
    );
    await vi.waitFor(() => expect(page.text()).toContain('Could not establish connection'));
    expect(page.text()).toContain('nudge ran into a problem');
  });

  test('closing a problem keeps it closed until the board changes', async () => {
    const page = open('unreadable-edited');
    await vi.waitFor(() => expect(page.panel.host.hidden).toBe(false));
    page.close();
    document.querySelector('[data-state="empty"]')!.setAttribute('data-state', 'tbd');
    await new Promise((settle) => setTimeout(settle, 20));
    expect(page.panel.host.hidden).toBe(true);
  });
});

describe('the switches', () => {
  const autoOff = { ...DEFAULTS, autoPanel: false };
  const settle = () => new Promise((done) => setTimeout(done, 20));

  test('"open after each guess" off: the hint is ready, and waits for the button', async () => {
    const page = open('mid-game', undefined, saved('mid-game').storage, autoOff);
    await vi.waitFor(() => expect(page.requests).toHaveLength(1));
    await settle();
    expect(page.panel.host.hidden).toBe(true);
    page.panel.toggle();
    expect(page.boxes()).toHaveLength(5);
    page.close();
    guess(troop!);
    await vi.waitFor(() => expect(page.requests).toHaveLength(2));
    await settle();
    expect(page.panel.host.hidden).toBe(true);
  });

  test('"open after each guess" off: an unreadable board waits for the button too', async () => {
    const page = open('unreadable-edited', undefined, saved('unreadable-edited').storage, autoOff);
    await settle();
    expect(page.panel.host.hidden).toBe(true);
    page.panel.toggle();
    expect(page.text()).toContain("nudge can't read the board");
  });

  test('a new strategy gives a new hint at once, and leaves a closed panel closed', async () => {
    const page = open('empty-hard-off');
    await vi.waitFor(() => expect(page.boxes()).toHaveLength(5));
    page.close();
    page.watcher!.setSettings({ ...DEFAULTS, strategy: 'entropy' });
    await vi.waitFor(() => expect(page.requests).toHaveLength(2));
    expect(page.requests[1]!.strategy).toBe('entropy');
    await settle();
    expect(page.panel.host.hidden).toBe(true);
    page.panel.toggle();
    page.panel.host.shadowRoot!.querySelector<HTMLButtonElement>('.actions button')!.click();
    expect(page.boxes()).toEqual(['t', 'a', 'r', 's', 'e']);
    const entropy = page.panel.host.shadowRoot!.querySelector<HTMLInputElement>(
      'input[aria-label="Entropy"]',
    )!;
    expect(entropy.checked).toBe(true);
  });

  test('switching "open after each guess" asks nothing new', async () => {
    const page = open('empty-hard-off');
    await vi.waitFor(() => expect(page.requests).toHaveLength(1));
    page.watcher!.setSettings(autoOff);
    await settle();
    expect(page.requests).toHaveLength(1);
  });

  test('after a win, the button shows that the game is over', async () => {
    const page = open('won');
    await settle();
    page.panel.toggle();
    expect(page.text()).toBe('This game is over.');
  });

  test('after a win, a new strategy leaves the opened panel open', async () => {
    const page = open('won');
    await settle();
    page.panel.toggle();
    page.watcher!.setSettings({ ...DEFAULTS, strategy: 'entropy' });
    await settle();
    expect(page.panel.host.hidden).toBe(false);
    expect(page.text()).toBe('This game is over.');
  });
});

describe('a board that is drawn after the page opens', () => {
  test('waits for it instead of calling it unreadable', async () => {
    document.body.innerHTML = '<main>loading</main>';
    const requests: HintRequest[] = [];
    const panel = createPanel(document);
    running = watchPage({
      document,
      storage: storageOf(saved('empty-hard-off').storage),
      panel,
      settings: { ...DEFAULTS, strategy: 'entropy' },
      ask: (request) => (requests.push(request), Promise.resolve(answer(nudge, request))),
    });
    await new Promise((settle) => setTimeout(settle, 20));
    expect(panel.host.hidden).toBe(true);
    document.querySelector('main')!.innerHTML = saved('empty-hard-off').boardHtml;
    await vi.waitFor(() => expect(requests).toHaveLength(1));
    await vi.waitFor(() => expect(panel.host.hidden).toBe(false));
  });

  test('but says so if it never comes', async () => {
    document.body.innerHTML = '<main>not Wordle after all</main>';
    const panel = createPanel(document);
    running = watchPage({
      document,
      storage: storageOf({}),
      panel,
      settings: DEFAULTS,
      ask: () => Promise.resolve({ kind: 'no-word' }),
      waitForBoardMs: 10,
    });
    await vi.waitFor(() =>
      expect(panel.host.shadowRoot!.querySelector('.body')!.textContent).toContain(
        'no "Row" elements',
      ),
    );
  });
});
