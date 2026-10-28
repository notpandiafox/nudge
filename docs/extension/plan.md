# Plan: the nudge Firefox extension (draft 2)

> Builds [the extension spec](spec.md), using the [spike's findings](spike.md)
> and the finished solver ([its plan](../wordle-solver/plan.md), §5.15 for the
> JavaScript interface). Shorter than the solver's plan on purpose: the hard
> problems (the algorithm, determinism, speed) are solved; this is wiring,
> reading NYT's page, and the panel.

## 1. Decisions

| # | Decision | Why |
|---|---|---|
| E-D1 | **The panel is drawn on NYT's page** (spec decision, confirmed after Firefox 149's change, below), not Firefox's toolbar popup | It stays put while the player types, and works on every supported Firefox |
| E-D2 | The solver runs in the extension's **background script**; the page script sends it the board and gets the hint back | The spike loaded it in an extension page in 4–10 ms; on the page itself it would fall under NYT's own security rules |
| E-D3 | The background answers each request with a **fresh solver**: create, replay the finished rows, `getBestGuess()`, `delete()` | Nothing to keep in sync, nothing lost when Firefox suspends the background script, no leaked solvers. Replaying at most 6 rows costs microseconds |
| E-D4 | **Manifest V3**, minimum Firefox 142 | The first version (desktop 140, Android 142) that knows the manifest's "collects no data" declaration; well behind the current extended-support release (153). (Draft 2 said 128, wrongly called the current ESR: 128 has been out of support since September 2025) |
| E-D5 | **TypeScript 6.0** (typescript-eslint supports up to 6.0, not 7), bundled with **esbuild**; the solver's `nudge.mjs` + `nudge.wasm` are copied in at build time from the WebAssembly build | One small, fast build tool; the solver stays built by its own CMake |
| E-D6 | Tests with **Vitest** + **jsdom** (a DOM in Node), on saved copies of NYT's page; the real `nudge.mjs` in Node for the solver tests | Vitest runs TypeScript directly; jsdom reads the saved copies like a browser would |
| E-D7 | **100% line and function coverage** of `extension/src`, like the solver | Same bar for the whole project; most code is pure functions that are easy to cover |
| E-D8 | The panel lives in a **shadow root** (its own little document inside NYT's page) | NYT's styles can't change the panel, and the panel's can't change NYT's page |
| E-D9 | Settings in `browser.storage.local`: `{ autoPanel: true, strategy: "minimax" }` | Remembered after Firefox closes, on this computer only (spec) |
| E-D10 | Exact versions for every npm package (`package-lock.json`, `npm ci` in CI) | Same reason as the solver's pinned tools: CI and your laptop run the same code |
| E-D11 | **CI/CD:** CI on every push (below); on a version tag (`v1.0.0`), CI also builds, gets Mozilla to sign the extension, and attaches the signed `.xpi` to a GitHub Release | Firefox installs only signed extensions permanently; a tag is the one manual step |
| E-D12 | **Tooling:** web-ext (run Firefox with the extension, `web-ext lint`, packaging and signing), ESLint with typescript-eslint, Prettier, a pre-commit hook (simple-git-hooks running `extension/scripts/pre-commit.mjs`: Prettier and ESLint on the changed extension files, clang-format on the changed C++ files), `@types/firefox-webext-browser`, and Dependabot for version updates. Not Playwright (it can't easily load extensions in Firefox) and not happy-dom (jsdom does that job) | The TypeScript side gets the same checks as the C++ side (clang-format, clang-tidy); the hook catches formatting before CI does, like the stray space in `ranking.hpp`. A small script instead of lint-staged, because lint-staged sees only the files under `extension/`, and that space was in `solver/` |

### E-D1: panel on the page, or the toolbar popup? (Decided: the page.)

The spec chose the page panel because a toolbar popup was thought to open only
on a click. **That changed:** since Firefox 149 (desktop), an extension may
open its toolbar popup by itself (`action.openPopup()`, Mozilla add-ons blog,
"WebExtensions API Changes (Firefox 149-152)", April 2026).

| | Panel on the page (spec) | Toolbar popup |
|---|---|---|
| Opens by itself after a guess | Yes, in every supported Firefox | Only on Firefox 149+ desktop |
| While the player types the next guess | Stays where it is | **Closes** as soon as the player clicks or types on the page (Firefox popups close when they lose focus), and opening it takes the keyboard away from the game |
| Position | Top right of the page | Under the toolbar button, top right of the window |
| Isolation from NYT's page | Shadow root (E-D8) | Complete: it's the extension's own page |
| Work | A bit more (drawing on NYT's page) | A bit less |

**Decided: the page panel.** A popup that opens after every guess would grab
the keyboard mid-game and vanish at the next click, which works against
"opens after each guess". The page panel can sit quietly and update.

## 2. How the pieces fit

```
 NYT Wordle page                                   nudge background script
 ┌─────────────────────────────────────┐           ┌─────────────────────────┐
 │ page script (content script)        │  board,   │ solver host             │
 │  readBoard(document)  ─────────┐    │  hard     │  loads nudge.mjs once   │
 │  readHardMode(localStorage)  ──┼──────mode,───▶ │  fresh Solver per ask   │
 │  watches the board for guesses │    │  strategy │  → hint or "no word"    │
 │  panel (shadow root) ◀─────────┘◀──────hint──── │                         │
 └─────────────────────────────────────┘           └─────────────────────────┘
        ▲ toolbar button: on Wordle it shows/hides the panel;
        └ anywhere else it opens the small "Nudge" popup (title + switch)
```

- **Toolbar button:** on Wordle tabs the button has no popup, so a click
  reaches the extension and toggles the panel; on every other tab it shows
  the "Nudge" popup. (`action.setPopup` per tab.)
- **Watching for guesses:** a `MutationObserver` on the board; a row counts
  once all five tiles have a color, so the reveal animation doesn't trigger
  it early.

### Code layout

```
extension/
├── manifest.json
├── package.json, package-lock.json, tsconfig.json, vitest.config.ts
├── build.mjs                    esbuild + copy nudge.mjs/nudge.wasm
├── src/
│   ├── content/main.ts          wires the page: observe, read, ask, draw
│   ├── content/readBoard.ts     pure: Document → rows, or unreadable
│   ├── content/readHardMode.ts  pure: saved data + rows on screen → hard mode, or unreadable
│   ├── content/panel.ts         draws a panel state into the shadow root
│   ├── shroud.ts                pure: hint + known greens → boxes; reveal one, reveal all
│   ├── background/solverHost.ts board + mode + strategy → hint or "no word"
│   ├── popup/                   the "Nudge" popup (title + switch)
│   ├── settings.ts              the two switches in browser.storage.local
│   └── messages.ts              the message types between the two sides
└── test/
    ├── fixtures/                saved copies of NYT's page (see E2)
    └── *.test.ts
```

*Why pure functions for reading and the shroud:* they take the page or the
data and return a result, so the tests feed them saved copies and check the
answer, with no browser at all. Only `main.ts` touches the live page.

## 3. Testing

- **Saved copies of NYT's page** (spec, "Tests"): the board's HTML and the
  page's saved settings and game entries, at known states. Account numbers
  are replaced with a fake one before they're committed.
- **Unit tests** on the pure functions with those copies, in jsdom.
- **The solver for real:** the solver-host tests load the real `nudge.mjs`
  in Node, so a hint test checks the actual move.
- **Hard mode:** every hint on a hard-mode copy is checked with an
  independent rule-B checker (the same idea as the solver's tests).
- **No network:** a test fails if any extension code calls `fetch`,
  `XMLHttpRequest` or `WebSocket` for anything but the extension's own files;
  the manifest asks for no other site.
- **In Firefox:** a short manual checklist on the live page (E9), because no
  test runner drives a Firefox extension on NYT's real site.

## 4. CI

A new `extension` job, after the `wasm` job (which uploads `nudge.mjs` and
`nudge.wasm` as an artifact): `npm ci`, Prettier's check, ESLint, type check
(`tsc --noEmit`), tests with the 100% coverage gate, `web-ext lint`
(Mozilla's own manifest and code checks), and the packaged extension as a
downloadable artifact. Dependabot watches `extension/package.json` and the
GitHub Actions versions.

### CD (on a version tag)

`web-ext sign` sends the packaged extension to addons.mozilla.org (AMO), which
signs it; the job attaches the signed `.xpi` to a GitHub Release for that tag.
It needs an AMO API key and secret, which the developer creates on AMO and
stores as GitHub repository secrets (`AMO_JWT_ISSUER`, `AMO_JWT_SECRET`); CI
reads them, nothing else does. Listed (public on AMO, reviewed by Mozilla) or
unlisted (signed, shared from GitHub Releases) is decided before E10.

## 5. Tasks, in order

One task per commit, like the solver's.

### E1. Scaffold
- `extension/` with the manifest, TypeScript, esbuild, Vitest, the copy of
  the solver files, the tools of E-D12 (web-ext, ESLint, Prettier, the
  pre-commit hook, the Firefox types, Dependabot), and the CI job.
- **Done when:** `web-ext run` opens Firefox with the empty extension loaded,
  a badly formatted file is stopped by the pre-commit hook, and CI is green
  (format, lint, type check, tests, coverage, `web-ext lint`).

### E2. Saved copies of NYT's page
- A small recorder (grown from the spike) that saves the board's HTML and
  the page's saved settings and game entries from the open Wordle tab.
- Copies: empty board, mid-game, won, lost, hard mode on and off, signed in
  and signed out at once (both entries), and one with a tile edited so its
  label disagrees (unreadable).
- **Done when:** the copies are committed with account numbers replaced.

### E3. Board reader
- `readBoard`: rows, or unreadable (spec, "Reading the board").
- **Done when:** every copy reads exactly; the edited copy is unreadable.

### E4. Hard-mode reader
- `readHardMode`: the game entry matching the rows on screen, else the
  settings entry, else unreadable.
- **Done when:** on, off, both entries, and missing entries all read right.

### E5. Solver host
- The background script and the messages: board, hard mode and strategy in;
  hint, "no word", or an error out.
- **Done when:** with the real `nudge.mjs`, hints match the solver; fallback
  and impossible boards give "no word"; hard-mode hints pass rule B.

### E6. The shroud
- `shroud.ts`: the boxes with only the known greens that fit; reveal one
  random covered box; reveal all.
- **Done when:** tests cover a probe hint whose letters break the greens, a
  hard-mode hint, and repeated "reveal a letter" until the word shows.

### E7. The panel on the page
- `panel.ts` and `main.ts`: draw each state (hint, "we don't know what the
  word is :C", "can't read the board" with the report link), open on page
  load and after each guess, stay quiet after a win or a loss.
- **Done when:** jsdom tests drive each state from the saved copies.

### E8. Toolbar button and switches
- The per-tab button behavior, the "Nudge" popup, the two switches,
  remembered.
- **Done when:** tests with a fake `browser.storage` show both switches
  survive a restart and a new strategy gives a new hint at once.

### E9. In Firefox, end to end
- The manual checklist on the live page; the 1-second check (from a guess
  to the hint being ready); no network requests in Firefox's network panel;
  the packaged extension. Delete `spike/firefox/`.
- **Done when:** every checklist item passes, with the numbers recorded here.

### E10. Release (CD)
- The tag-triggered job: build, `web-ext sign`, GitHub Release with the
  signed `.xpi`. Version number from `manifest.json`, checked against the tag.
- **Done when:** a test tag (`v0.1.0`) produces a signed `.xpi` that installs
  in a normal Firefox and keeps working after a restart.

## 6. Risks

| Risk | What happens | Answer |
|---|---|---|
| NYT changes the board's HTML | Reader finds nothing or disagrees | Detected and reported (E3); a new saved copy shows what changed |
| NYT renames its saved-data entries | Hard mode unreadable | Same "can't read" message (E4) |
| Firefox suspends the background script | Next hint reloads the solver | ~10 ms; nothing to restore (E-D3) |
| NYT's page styles leak into the panel | Panel looks broken | Shadow root (E-D8) |

## 7. Decided, and still to decide

Decided (draft 2): the page panel (E-D1); TypeScript, esbuild, Vitest and
jsdom; minimum Firefox 128; 100% coverage; CI/CD (E-D11, E10); the tooling
of E-D12.

Still to decide, before E10: listed on AMO (addons.mozilla.org, Mozilla's
add-ons site: public, reviewed) or unlisted (signed by AMO, shared from
GitHub Releases).
