// The page script on NYT's Wordle page (plan §2, E8): watches the board and
// draws the panel; the background script answers its hint requests and runs
// the toolbar button.
import type { HintResponse, Toggle, WordlePage } from '../messages';
import { loadSettings } from '../settings';
import { watchPage } from './page';
import { createPanel } from './panel';
import { watchTheme } from './theme';

const saved = browser.storage.local;
// A flipped switch is only saved; the change comes back below, as it does in
// every other Wordle tab.
const panel = createPanel(document, { onSettings: (change) => void saved.set(change) });

browser.runtime.onMessage.addListener((message: Partial<Toggle> | undefined) => {
  if (message?.type === 'toggle') panel.toggle();
});
// Firefox gives the tab its "Nudge" popup back on every new page, so say this
// again when Back brings Wordle back from the back-forward cache (the script
// doesn't start again then).
const hello = () => void browser.runtime.sendMessage({ type: 'wordle-page' } satisfies WordlePage);
hello();
addEventListener('pageshow', (event) => {
  if (event.persisted) hello();
});

// Light or dark like Wordle; saved for the popup, which opens away from Wordle.
watchTheme({
  document,
  systemDark: matchMedia('(prefers-color-scheme: dark)'),
  panel,
  save: (theme) => void saved.set({ theme }),
});

void loadSettings(saved).then((settings) => {
  const page = watchPage({
    document,
    storage: localStorage, // NYT's saved data: the content script shares the page's origin
    panel,
    settings,
    ask: (request) => browser.runtime.sendMessage(request) as Promise<HintResponse>,
  });
  saved.onChanged.addListener(() => void loadSettings(saved).then(page.setSettings));
});
