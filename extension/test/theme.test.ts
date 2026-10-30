// @vitest-environment jsdom
// Light or dark, like Wordle (plan E9, E-D13; spec "How the panel looks"):
// read from the colour behind the board, as the player sees it.
import { readFileSync } from 'node:fs';
import { afterEach, beforeEach, describe, expect, test, vi } from 'vitest';

import { createPanel } from '../src/content/panel';

import { backgroundBehindBoard, readTheme, watchTheme, type Theme } from '../src/content/theme';

const board = '<div aria-label="Row 1"><div data-state="empty"></div></div>';

beforeEach(() => {
  document.body.removeAttribute('style');
  document.documentElement.removeAttribute('style');
});

// The page, with a background colour on the board's container.
function page(behindBoard: string, bodyColour = '') {
  document.body.innerHTML = `<main style="background-color: ${behindBoard}"><div>${board}</div></main>`;
  document.body.style.backgroundColor = bodyColour;
}

describe('readTheme', () => {
  test("Wordle's dark page (#121213) is dark", () => {
    page('#121213');
    expect(readTheme(document, false)).toBe('dark');
  });

  test("Wordle's light page (white) is light, whatever Firefox's own setting", () => {
    page('#ffffff');
    expect(readTheme(document, true)).toBe('light');
  });

  test('is decided by brightness, at half', () => {
    page('rgb(120, 120, 120)');
    expect(readTheme(document, false)).toBe('dark');
    page('rgb(140, 140, 140)');
    expect(readTheme(document, true)).toBe('light');
  });

  test('weighs green as the eye does: a bright green page is light', () => {
    page('rgb(0, 200, 0)');
    expect(readTheme(document, true)).toBe('light');
  });

  test('looks past see-through elements to the colour behind them', () => {
    page('transparent', '#121213');
    expect(readTheme(document, false)).toBe('dark');
    page('rgba(255, 255, 255, 0)', 'rgb(255, 255, 255)');
    expect(readTheme(document, true)).toBe('light');
  });

  test('a half see-through colour still counts', () => {
    page('rgba(18, 18, 19, 0.5)', '#ffffff');
    expect(readTheme(document, false)).toBe('dark');
  });

  test('with no colour anywhere, follows Firefox', () => {
    page('transparent');
    expect(readTheme(document, true)).toBe('dark');
    expect(readTheme(document, false)).toBe('light');
  });

  test('before the board is drawn, the page behind it decides', () => {
    document.body.innerHTML = '<main>loading</main>';
    document.body.style.backgroundColor = '#121213';
    expect(readTheme(document, false)).toBe('dark');
  });
});

describe('backgroundBehindBoard', () => {
  test('is the colour the recorder keeps: a colour, nothing else from the page', () => {
    page('transparent', '#121213');
    expect(backgroundBehindBoard(document)).toBe('rgb(18, 18, 19)');
    page('transparent');
    expect(backgroundBehindBoard(document)).toBeNull();
  });
});

describe('watchTheme', () => {
  // Firefox's own light or dark setting, switchable by the test.
  function firefox(dark: boolean) {
    const listeners: (() => void)[] = [];
    return {
      matches: dark,
      addEventListener: (_type: 'change', listener: () => void) => void listeners.push(listener),
      removeEventListener: () => {},
      switchTo(next: boolean) {
        this.matches = next;
        for (const listener of listeners) listener();
      },
    };
  }

  let stop: (() => void) | undefined;
  afterEach(() => stop?.());

  function watch(system = firefox(false)) {
    const shown: Theme[] = [];
    const saved: Theme[] = [];
    stop = watchTheme({
      document,
      systemDark: system,
      panel: { setTheme: (theme) => shown.push(theme) },
      save: (theme) => saved.push(theme),
    });
    return { shown, saved };
  }

  test('sets the theme on the panel when the page opens, and saves it', () => {
    page('transparent', '#121213');
    const { shown, saved } = watch();
    expect(shown).toEqual(['dark']);
    expect(saved).toEqual(['dark']);
  });

  test("follows Wordle's switch at once, saving once per change", async () => {
    page('transparent', '#121213');
    const { shown, saved } = watch();
    document.body.style.backgroundColor = '#ffffff'; // Wordle's switch, turned to light
    await vi.waitFor(() => expect(shown).toEqual(['dark', 'light']));
    document.body.className = 'something-unrelated';
    document.documentElement.setAttribute('style', 'scroll-behavior: smooth');
    await new Promise((settle) => setTimeout(settle, 20));
    expect(saved).toEqual(['dark', 'light']);
  });

  test('notices a theme set on <html>', async () => {
    page('transparent');
    const { shown } = watch();
    document.documentElement.style.backgroundColor = '#121213';
    await vi.waitFor(() => expect(shown).toEqual(['light', 'dark']));
  });

  test('follows Firefox switching to dark when the page has no colour of its own', () => {
    page('transparent');
    const system = firefox(false);
    const { shown } = watch(system);
    system.switchTo(true);
    expect(shown).toEqual(['light', 'dark']);
  });

  test('reads it again once the board is drawn', async () => {
    document.body.innerHTML = '<main>loading</main>';
    document.body.style.backgroundColor = '#121213';
    const { shown } = watch();
    document.querySelector('main')!.outerHTML =
      `<main style="background-color: #ffffff">${board}</main>`;
    await vi.waitFor(() => expect(shown).toEqual(['dark', 'light']));
  });

  test('stops when told to', async () => {
    page('transparent', '#121213');
    const { shown } = watch();
    stop!();
    document.body.style.backgroundColor = '#ffffff';
    await new Promise((settle) => setTimeout(settle, 20));
    document.body.style.backgroundColor = '#121213';
    document.querySelector('main')!.outerHTML =
      `<main style="background-color: #ffffff">${board}</main>`;
    await new Promise((settle) => setTimeout(settle, 20));
    expect(shown).toEqual(['dark']);
  });
});

// The copies recorded on the live page, with the colour behind the board put
// back behind it (jsdom has no NYT style sheets).
describe.each([
  ['won-light', 'light'],
  ['won-dark-theme', 'dark'],
])('the saved copy %s', (name, theme) => {
  test(`gives a ${theme} panel, whatever Firefox's own setting`, () => {
    const copy = JSON.parse(readFileSync(`test/fixtures/wordle-2026-10-30-${name}.json`, 'utf8'));
    document.body.innerHTML = `<main style="background-color: ${copy.background}">${copy.boardHtml}</main>`;
    for (const firefoxDark of [false, true]) {
      const panel = createPanel(document);
      const stopWatching = watchTheme({
        document,
        systemDark: {
          matches: firefoxDark,
          addEventListener: () => {},
          removeEventListener: () => {},
        },
        panel,
        save: () => {},
      });
      expect(panel.host.getAttribute('theme')).toBe(theme);
      stopWatching();
    }
  });
});
