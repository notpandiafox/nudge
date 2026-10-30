// @vitest-environment jsdom
// The page script as Firefox starts it (plan E7, E8): NYT's own saved data,
// the switches from extension storage, and messages to and from the
// background script.
import { readFileSync } from 'node:fs';
import { describe, expect, test, vi } from 'vitest';

import { fakeStorage } from './fakeStorage';

const fixture = JSON.parse(
  readFileSync('test/fixtures/wordle-2026-10-30-empty-hard-on.json', 'utf8'),
);
document.body.innerHTML = `<main>${fixture.boardHtml}</main>`;
for (const [key, value] of Object.entries(fixture.storage)) {
  localStorage.setItem(key, JSON.stringify(value));
}

const sent: { type: string; strategy?: string }[] = [];
let fromBackground: ((message: unknown) => void) | undefined;
const storage = fakeStorage({ strategy: 'entropy' }); // saved in an earlier visit
vi.stubGlobal('matchMedia', () => ({
  matches: true, // Firefox is dark; this copy of the page has no colours of its own
  addEventListener: () => {},
  removeEventListener: () => {},
}));
vi.stubGlobal('browser', {
  storage,
  runtime: {
    sendMessage: async (message: { type: string }) => {
      sent.push(message);
      return message.type === 'hint' ? { kind: 'no-word' } : undefined;
    },
    onMessage: { addListener: (added: typeof fromBackground) => (fromBackground = added) },
  },
});
await import('../src/content/main');

const panel = () => document.querySelector('nudge-panel')!;
const input = (name: string) =>
  panel().shadowRoot!.querySelector<HTMLInputElement>(`input[aria-label="${name}"]`)!;

describe('the page script', () => {
  test('tells the background script this tab is Wordle', () => {
    expect(sent[0]).toEqual({ type: 'wordle-page' });
  });

  // Firefox gives the tab its popup back on every new page, Back included;
  // a page restored from the back-forward cache doesn't start the script again.
  test('tells it again when the page comes back from the back-forward cache', () => {
    const hellos = () => sent.filter((message) => message.type === 'wordle-page').length;
    const pageshow = (persisted: boolean) =>
      window.dispatchEvent(Object.assign(new Event('pageshow'), { persisted }));
    pageshow(false); // the first load: already said
    expect(hellos()).toBe(1);
    pageshow(true);
    expect(hellos()).toBe(2);
  });

  test('asks with hard mode from the page and the saved strategy, and shows the answer', async () => {
    await vi.waitFor(() =>
      expect(sent).toContainEqual({ type: 'hint', rows: [], hardMode: true, strategy: 'entropy' }),
    );
    await vi.waitFor(() =>
      expect(panel().shadowRoot!.textContent).toContain("we don't know what the word is :C"),
    );
    expect(input('Entropy').checked).toBe(true);
  });

  test("sets the panel's theme and saves it for the popup", () => {
    expect(panel().getAttribute('theme')).toBe('dark');
    expect(storage.saved.theme).toBe('dark');
  });

  test("Wordle's theme switch changes the panel at once, and asks for no new hint", async () => {
    const asked = sent.filter((message) => message.type === 'hint').length;
    document.body.style.backgroundColor = '#ffffff';
    await vi.waitFor(() => expect(panel().getAttribute('theme')).toBe('light'));
    expect(storage.saved.theme).toBe('light');
    await new Promise((settle) => setTimeout(settle, 20));
    expect(sent.filter((message) => message.type === 'hint')).toHaveLength(asked);
  });

  test('the toolbar button shows and hides the panel', () => {
    fromBackground!({ type: 'toggle' });
    expect(panel().hasAttribute('hidden')).toBe(true);
    fromBackground!({ type: 'something-else' });
    expect(panel().hasAttribute('hidden')).toBe(true);
    fromBackground!({ type: 'toggle' });
    expect(panel().hasAttribute('hidden')).toBe(false);
  });

  test('a flipped switch is saved, and a new strategy asks again at once', async () => {
    input('Minimax').click();
    expect(storage.saved.strategy).toBe('minimax');
    await vi.waitFor(() =>
      expect(sent.at(-1)).toMatchObject({ type: 'hint', strategy: 'minimax' }),
    );
    input('Open after each guess').click();
    expect(storage.saved.autoPanel).toBe(false);
  });
});
