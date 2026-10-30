// The two switches (plan E-D9; spec "Switches"), kept in browser.storage.local:
// remembered after Firefox closes, on this computer only. Saving is just
// `storage.set({ autoPanel })` or `storage.set({ strategy })`.
import type { Strategy } from './messages';

export interface Settings {
  autoPanel: boolean;
  strategy: Strategy;
}

export const DEFAULTS: Settings = { autoPanel: true, strategy: 'minimax' };

// What's saved is checked, not trusted: a value of the wrong kind (an older
// version, or a hand edit) gives the default for that switch.
export async function loadSettings(
  storage: Pick<browser.storage.StorageArea, 'get'>,
): Promise<Settings> {
  const { autoPanel, strategy } = await storage.get(['autoPanel', 'strategy']);
  return {
    autoPanel: typeof autoPanel === 'boolean' ? autoPanel : DEFAULTS.autoPanel,
    strategy: strategy === 'minimax' || strategy === 'entropy' ? strategy : DEFAULTS.strategy,
  };
}
