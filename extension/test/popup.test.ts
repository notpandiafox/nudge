// @vitest-environment jsdom
// The toolbar's "Nudge" popup, on every tab but Wordle (plan E8, E9; spec
// "What the panel shows", "How the panel looks"): the name and the
// automatic-panel switch, nothing else, in the last theme Wordle had.
import { readFileSync } from 'node:fs';
import { describe, expect, test, vi } from 'vitest';

import { fakeStorage } from './fakeStorage';

const html = readFileSync('src/popup/popup.html', 'utf8');

// Opens a fresh popup over this saved data.
async function openPopup(saved: Record<string, unknown>) {
  document.documentElement.removeAttribute('theme');
  document.body.innerHTML = new DOMParser().parseFromString(html, 'text/html').body.innerHTML;
  const storage = fakeStorage(saved);
  vi.stubGlobal('browser', { storage });
  vi.resetModules();
  await import('../src/popup/popup');
  const toggle = document.querySelector<HTMLInputElement>('input')!;
  return { storage, toggle };
}

describe('the popup', () => {
  test('shows "Nudge" and the automatic-panel switch, nothing else', async () => {
    const { toggle } = await openPopup({});
    expect(document.querySelector('h1')!.textContent).toBe('Nudge');
    expect(document.querySelectorAll('input, button, select')).toHaveLength(1);
    expect(toggle.getAttribute('role')).toBe('switch');
    expect(toggle.labels![0]!.textContent).toContain('Open after each guess');
  });

  test('shows the saved switch, and saves it when flipped', async () => {
    const { storage, toggle } = await openPopup({ autoPanel: false });
    await vi.waitFor(() => expect(toggle.checked).toBe(false));
    toggle.click();
    expect(storage.saved.autoPanel).toBe(true);
  });
});

describe("the popup's theme", () => {
  const theme = () => document.documentElement.getAttribute('theme');

  test('is the last one Wordle had, set as soon as it is read', async () => {
    await openPopup({ theme: 'dark' });
    await vi.waitFor(() => expect(theme()).toBe('dark'));
    await openPopup({ theme: 'light' });
    await vi.waitFor(() => expect(theme()).toBe('light'));
  });

  // Firefox's own setting needs no script, so the popup is never painted in
  // the wrong colours first.
  test("before Wordle has been seen (or saved wrong), is left to Firefox's", async () => {
    await openPopup({});
    await new Promise((settle) => setTimeout(settle, 20));
    expect(theme()).toBeNull();
    await openPopup({ theme: 'purple' });
    await new Promise((settle) => setTimeout(settle, 20));
    expect(theme()).toBeNull();
  });

  test("has dark colours like Wordle's, for a saved dark theme and for Firefox's", () => {
    expect(html).toMatch(/:root\[theme='dark'\][^}]*--page: #121213/);
    expect(html).toMatch(
      /@media \(prefers-color-scheme: dark\) \{\s*:root:not\(\[theme='light'\]\)[^}]*--page: #121213/,
    );
  });
});
