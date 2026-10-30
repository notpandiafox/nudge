// Runs on the Wordle page; answers the popup with a test copy of the board.
// All the deciding is in fixture.ts, which the tests cover.
import { makeFixture } from './fixture';

browser.runtime.onMessage.addListener((message: { type?: string; note?: string }) => {
  if (message.type !== 'record') return undefined;
  return Promise.resolve(
    makeFixture({
      document,
      storage: localStorage,
      href: location.href,
      note: message.note ?? '',
      now: new Date(),
    }),
  );
});
