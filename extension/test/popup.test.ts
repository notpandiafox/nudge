// @vitest-environment jsdom
// The toolbar's "Nudge" popup, on every tab but Wordle (plan E8; spec "What the
// panel shows"): the name and the automatic-panel switch, nothing else.
import { readFileSync } from 'node:fs';
import { expect, test, vi } from 'vitest';

import { fakeStorage } from './fakeStorage';

const html = readFileSync('src/popup/popup.html', 'utf8');
document.body.innerHTML = new DOMParser().parseFromString(html, 'text/html').body.innerHTML;
const storage = fakeStorage({ autoPanel: false });
vi.stubGlobal('browser', { storage });
await import('../src/popup/popup');

const toggle = () => document.querySelector<HTMLInputElement>('input')!;

test('shows "Nudge" and the automatic-panel switch, nothing else', () => {
  expect(document.querySelector('h1')!.textContent).toBe('Nudge');
  expect(document.querySelectorAll('input, button, select')).toHaveLength(1);
  expect(toggle().getAttribute('role')).toBe('switch');
  expect(toggle().labels![0]!.textContent).toContain('Open after each guess');
});

test('shows the saved switch, and saves it when flipped', async () => {
  await vi.waitFor(() => expect(toggle().checked).toBe(false));
  toggle().click();
  expect(storage.saved.autoPanel).toBe(true);
});
