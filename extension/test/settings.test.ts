// The two switches, kept in extension storage (plan E8, E-D9; spec "Switches").
import { describe, expect, test } from 'vitest';

import { DEFAULTS, loadSettings } from '../src/settings';
import { fakeStorage } from './fakeStorage';

describe('the switches', () => {
  test('start as automatic panel on, strategy minimax', async () => {
    expect(DEFAULTS).toEqual({ autoPanel: true, strategy: 'minimax' });
    expect(await loadSettings(fakeStorage().local)).toEqual(DEFAULTS);
  });

  test('survive a restart', async () => {
    const before = fakeStorage();
    await before.local.set({ autoPanel: false, strategy: 'entropy' });
    expect(await loadSettings(fakeStorage(before.saved).local)).toEqual({
      autoPanel: false,
      strategy: 'entropy',
    });
  });

  test('a saved value of the wrong kind falls back to the default, the other one stays', async () => {
    expect(await loadSettings(fakeStorage({ autoPanel: 'no', strategy: 'entropy' }).local)).toEqual(
      { autoPanel: true, strategy: 'entropy' },
    );
    expect(await loadSettings(fakeStorage({ autoPanel: false, strategy: 'random' }).local)).toEqual(
      { autoPanel: false, strategy: 'minimax' },
    );
  });
});
