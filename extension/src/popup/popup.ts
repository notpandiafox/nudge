// The toolbar's "Nudge" popup (plan E8, E9), on every tab but Wordle: the name
// and the automatic-panel switch, in the last theme Wordle had. Wordle tabs
// have no popup; the button toggles the panel there instead.
import { loadSettings, loadTheme } from '../settings';

// Without a saved theme, the popup's CSS follows Firefox's own setting.
void loadTheme(browser.storage.local).then((saved) => {
  if (saved) document.documentElement.setAttribute('theme', saved);
});

const auto = document.querySelector<HTMLInputElement>('#auto')!;
void loadSettings(browser.storage.local).then((settings) => (auto.checked = settings.autoPanel));
auto.addEventListener('change', () => void browser.storage.local.set({ autoPanel: auto.checked }));
