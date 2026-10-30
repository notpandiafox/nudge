// A stand-in for browser.storage: `local` keeps everything in a plain object
// (so a "restart" is just reading it again), and its `onChanged` tells
// listeners about every write, as Firefox tells every part of the extension.
type Listener = (changes: Record<string, unknown>) => void;

export function fakeStorage(saved: Record<string, unknown> = {}) {
  const listeners: Listener[] = [];
  return {
    saved,
    local: {
      get: async (keys: string[]) =>
        Object.fromEntries(keys.filter((key) => key in saved).map((key) => [key, saved[key]])),
      set: async (items: Record<string, unknown>) => {
        Object.assign(saved, items);
        for (const listener of listeners) listener(items);
      },
      onChanged: { addListener: (listener: Listener) => void listeners.push(listener) },
    },
  };
}
