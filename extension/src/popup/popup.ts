// The toolbar's "Nudge" popup (plan E8), on every tab but Wordle: the name and
// the automatic-panel switch. Wordle tabs have no popup; the button toggles
// the panel there instead.
import { loadSettings } from '../settings';

const auto = document.querySelector<HTMLInputElement>('#auto')!;
void loadSettings(browser.storage.local).then((settings) => (auto.checked = settings.autoPanel));
auto.addEventListener('change', () => void browser.storage.local.set({ autoPanel: auto.checked }));
