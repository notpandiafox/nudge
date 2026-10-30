// @vitest-environment jsdom
// The page script as Firefox starts it (plan E7): NYT's own saved data, and
// hint requests sent to the background script.
import { readFileSync } from 'node:fs';
import { expect, test, vi } from 'vitest';

import type { HintRequest } from '../src/messages';

const fixture = JSON.parse(
  readFileSync('test/fixtures/wordle-2026-10-30-empty-hard-on.json', 'utf8'),
);
document.body.innerHTML = `<main>${fixture.boardHtml}</main>`;
for (const [key, value] of Object.entries(fixture.storage)) {
  localStorage.setItem(key, JSON.stringify(value));
}
const sent: HintRequest[] = [];
vi.stubGlobal('browser', {
  runtime: {
    sendMessage: (request: HintRequest) => (
      sent.push(request),
      Promise.resolve({ kind: 'no-word' })
    ),
  },
});
await import('../src/content/main');

test('asks the background script, with hard mode from the page, and shows the answer', async () => {
  expect(sent).toEqual([{ type: 'hint', rows: [], hardMode: true, strategy: 'minimax' }]);
  const panel = document.querySelector('nudge-panel')!;
  await vi.waitFor(() =>
    expect(panel.shadowRoot!.textContent).toContain("we don't know what the word is :C"),
  );
});
