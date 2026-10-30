// The background script (plan E5): answers the page's hint requests with the
// real solver, loaded from the extension's own files.
import { pathToFileURL } from 'node:url';
import { describe, expect, test, vi } from 'vitest';

type Listener = (message: unknown) => Promise<unknown> | undefined;

const solverDir = process.env.NUDGE_WASM_DIR ?? '../build-wasm/solver/wasm';
let listener: Listener | undefined;
// A plain list, not a mock: mock call records can be cleared between tests.
const loaded: string[] = [];
const getURL = (path: string) => {
  loaded.push(path);
  return pathToFileURL(`${solverDir}/${path}`).href;
};
vi.stubGlobal('browser', {
  runtime: {
    getURL,
    onMessage: { addListener: (added: Listener) => (listener = added) },
  },
});
await import('../src/background/main');

describe('the background script', () => {
  test('answers a hint request with the solver', async () => {
    const response = await listener!({
      type: 'hint',
      rows: [],
      hardMode: false,
      strategy: 'entropy',
    });
    expect(response).toEqual({ kind: 'hint', word: 'tarse' });
  });

  test('loads the solver from the extension once, not per request', async () => {
    await listener!({ type: 'hint', rows: [], hardMode: false, strategy: 'minimax' });
    expect(loaded).toEqual(['nudge.mjs']);
  });

  test('leaves other messages to others', () => {
    expect(listener!({ type: 'something-else' })).toBeUndefined();
    expect(listener!(undefined)).toBeUndefined();
  });
});

describe('when the solver fails to load', () => {
  test('answers with an error, and tries again on the next request', async () => {
    // A fresh copy of the background script whose first load finds no file.
    let missing = true;
    let fresh: Listener | undefined;
    vi.stubGlobal('browser', {
      runtime: {
        getURL: (path: string) =>
          pathToFileURL(`${solverDir}/${missing ? 'missing-' : ''}${path}`).href,
        onMessage: { addListener: (added: Listener) => (fresh = added) },
      },
    });
    vi.resetModules();
    await import('../src/background/main');
    const request = { type: 'hint', rows: [], hardMode: false, strategy: 'minimax' };

    expect(await fresh!(request)).toMatchObject({ kind: 'error' });
    missing = false;
    expect(await fresh!(request)).toEqual({ kind: 'hint', word: 'olate' });
  });
});
