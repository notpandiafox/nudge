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
| E-D9 | Settings in `browser.storage.local`: `{ autoPanel: true, strategy: "minimax" }`, plus `theme` (`"light"` or `"dark"`), the last Wordle theme seen (E9) | Remembered after Firefox closes, on this computer only (spec) |
| E-D10 | Exact versions for every npm package (`package-lock.json`, `npm ci` in CI) | Same reason as the solver's pinned tools: CI and your laptop run the same code |
| E-D11 | **CI/CD:** CI on every push (below); on a version tag (`v1.0.0`), CI also builds, gets Mozilla to sign the extension, and attaches the signed `.xpi` to a GitHub Release | Firefox installs only signed extensions permanently; a tag is the one manual step |
| E-D12 | **Tooling:** web-ext (run Firefox with the extension, `web-ext lint`, packaging and signing), ESLint with typescript-eslint, Prettier, a pre-commit hook (simple-git-hooks running `extension/scripts/pre-commit.mjs`: Prettier and ESLint on the changed extension files, clang-format on the changed C++ files), `@types/firefox-webext-browser`, and Dependabot for version updates. Not Playwright (it can't easily load extensions in Firefox) and not happy-dom (jsdom does that job) | The TypeScript side gets the same checks as the C++ side (clang-format, clang-tidy); the hook catches formatting before CI does, like the stray space in `ranking.hpp`. A small script instead of lint-staged, because lint-staged sees only the files under `extension/`, and that space was in `solver/` |
| E-D13 | **Dark mode is read from the page's colours**, not from a class name or NYT's saved settings: the background colour of the nearest element behind the board that has one; dark when its brightness is below half. No board yet: Firefox's own setting (`prefers-color-scheme`) | It is what the player sees, whichever way Wordle got dark (its own switch, or following Firefox); it survives NYT renaming classes, like the board reader (E3); and it reads nothing new from saved data |

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
│   ├── content/main.ts          starts page.ts with the real page, storage and messaging
│   ├── content/page.ts          the loop: observe, read, ask, draw
│   ├── content/readBoard.ts     pure: Document → rows, or unreadable
│   ├── content/readHardMode.ts  pure: saved data + rows on screen → hard mode, or unreadable
│   ├── content/panel.ts         draws a panel state into the shadow root, light or dark
│   ├── content/theme.ts         pure: the page → light or dark (E-D13)
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
- **In Firefox:** a short manual checklist on the live page (E10), because no
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
unlisted (signed, shared from GitHub Releases) is decided before E11.

## 5. Tasks, in order

One task per commit, like the solver's.

### E1. Scaffold
- `extension/` with the manifest, TypeScript, esbuild, Vitest, the copy of
  the solver files, the tools of E-D12 (web-ext, ESLint, Prettier, the
  pre-commit hook, the Firefox types, Dependabot), and the CI job.
- **Done when:** `web-ext run` opens Firefox with the empty extension loaded,
  a badly formatted file is stopped by the pre-commit hook, and CI is green
  (format, lint, type check, tests, coverage, `web-ext lint`).

**Done in E1.** `extension/` builds, tests and packages, and CI's `extension`
job is green on `main` (23 seconds: format, lint, type check, build, tests
with the coverage gate, `web-ext lint`, package). `web-ext run` opens Firefox
with nudge loaded (its button starts in Firefox's Extensions menu, the
puzzle piece).

- The manifest test came first: six checks of the privacy promises (only the
  Wordle page, only storage, no data collected, WebAssembly but no `eval`)
  and the version shared with `package.json`. Three mutations (an extra
  permission, `unsafe-eval`, every site) each fail it.
- **Firefox 142, not 128** (E-D4): 128 has been out of support since
  September 2025, and the manifest's "collects no data" key needs 140 on
  desktop and 142 on Android.
- **TypeScript 6.0, not 7** (E-D5), and Dependabot ignores TypeScript 6.1 and
  later: typescript-eslint supports below 6.1 only.
- **Node 24** is required (`engines`): the current Vitest, jsdom and ESLint
  don't run on Node 20, which is out of support.
- **The pre-commit hook** is a script, not lint-staged (E-D12). Tested with a
  separate git index, so the real staging area was never touched: badly
  formatted TypeScript and C++ each stop the commit, the fixed files pass.
  It calls Prettier and ESLint through Node, with no shell (Node 24 warns
  about shell arguments). npm 11 blocks packages' install scripts, so the
  hook is installed by `npm install`'s `prepare` step or by
  `npx simple-git-hooks`.
- **Installed versions** are exact and at least two weeks old
  (`npm install --before`); Dependabot uses the same 14-day cooldown.
  `npm audit fix` took the safe fixes; what's left (shell-quote, deepmerge)
  is inside web-ext, a dev tool that never ships, and npm's only offer is a
  years-old web-ext.
- On Windows PowerShell, call npm as `npm.cmd` (`npm.cmd start`) unless
  scripts are allowed (`Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`).
- **Open:** the icon (`assets/nudge_icon.svg`) is 169 × 128; `web-ext lint`
  warns that icons must be square. Fix before E11.

### E2. Saved copies of NYT's page
- A small recorder (grown from the spike) that saves the board's HTML and
  the page's saved settings and game entries from the open Wordle tab.
- Copies: empty board, mid-game, won, lost, hard mode on and off, signed in
  and signed out at once (both entries), and one with a tile edited so its
  label disagrees (unreadable).
- **Done when:** the copies are committed with account numbers replaced.

**Done in E2.** Six saved copies of NYT's page are in
`extension/test/fixtures/`, recorded on 2026-10-30 from the developer's
Firefox: empty board with hard mode off and on, mid-game (3 guesses), won in
6, and two edited from those (lost: last row changed and status `FAIL`;
unreadable: one tile's letter disagrees with its label). Edited copies say so
in their note.

- **The recorder** (`extension/recorder/`, built into `dist-recorder/`, never
  shipped) saves the rows and their tiles, and from NYT's saved data only the
  two Wordle entries, by allowlist: `hardMode`, `currentRowIndex`, `status`,
  `puzzleId`, `schemaVersion`. Guesses, answers, stats and anything unknown
  are dropped; account numbers become fake ones (`10000001`, …).
- **Found while testing it:** saving "the smallest element holding every row"
  can be the whole page (with names, menus and an email address); it now
  saves the rows only, and a test pins that case.
- **A second, independent guard** (`test/fixtures.test.ts`) checks every
  committed copy: only the two entries with fake numbers, no field outside
  the allowlist, no account-like number or email anywhere. Checked on a
  deliberately leaky copy (fails all three privacy checks) and a clean one
  (passes); that check first caught the guard itself being too strict.
- **For E4, from the real data:** the signed-in entry held an *old* game
  (another puzzle, won, 5 rows) while today's game was in `ANON`. So the
  matching rule (the entry whose rows match the board) is needed for real,
  and these copies test it: today's entry says 0, 3 or 6 rows, the old one
  always 5.
- Recording needs the developer's normal Firefox (Load Temporary Add-on, from
  `dist-recorder/manifest.json`): `web-ext run` starts a fresh profile with
  no saved games.

### E3. Board reader
- `readBoard`: rows, or unreadable (spec, "Reading the board").
- **Done when:** every copy reads exactly; the edited copy is unreadable.

**Done in E3.** `readBoard` (`src/content/readBoard.ts`) reads every saved
copy exactly: the empty board, mid-game (`adieu`, `strow`, `smart`), won and
lost, including `motts` (one T green, the other gray). It reports why it can't
read a board, which the panel can show with the report link (E7).

- Tests run on the saved copies, with NYT's menu buttons (which also carry
  `data-state`) placed in front of the rows, the case that misread the spike.
- **Unreadable:** a tile whose letter or colour disagrees with its spoken
  label (the edited copy, and a colour edit), a row without five tiles, no
  rows at all, and **rows with no tiles at all**. That last one goes beyond
  the spec's "finished row": if NYT renamed the colour attribute, checking
  only finished rows would read a mid-game board as empty and hint the
  opener. The spec now says any row.
- **A colour value it doesn't know** is unreadable too (found in the E3
  review): only `empty` and `tbd` mean "not finished yet". Before, any
  unknown value skipped the row, so if NYT renamed `correct`, every row with
  a green tile vanished and the board read as almost empty.
- Rows being typed, or turning over one tile at a time, are ignored until
  every tile has a colour.
- Mutation checks, each caught: the letter or the colour not checked against
  the label, the five-tile check removed, a half-turned row counted, and the
  spike's own bug (tiles taken across the whole page), which fails 8 tests.
- Under Vitest's jsdom environment, `import.meta.url` doesn't point at the
  test file, so the saved copies are loaded by a path relative to
  `extension/`, where the tests run.

### E4. Hard-mode reader
- `readHardMode`: the game entry matching the rows on screen, else the
  settings entry, else unreadable.
- **Done when:** on, off, both entries, and missing entries all read right.

**Done in E4.** `readHardMode` (`src/content/readHardMode.ts`) reads every
saved copy right, including the real two-entry case: in `empty-hard-on`, the
signed-in entry holds an old game (5 rows, off) and the two settings disagree
(signed in off, signed out on), and today's game (signed out, 0 rows, on)
decides.

- Order: the game whose played rows match the finished rows on screen; else
  the settings when they agree; else unreadable. It reads only `hardMode` and
  the row count, and skips any entry shaped differently instead of trusting
  or failing on it.
- **Disagreement is unreadable**, never a guess: two matching games that
  disagree, or settings that disagree with no matching game. The spec now
  says so.
- A test for today's game beating the settings when they differ (a game
  started in hard mode, with hard mode switched off since) was added after
  noticing that no saved copy would catch the order being swapped.
- Mutation checks, each caught: rows not matched, games ignored, settings
  before the game, the first value winning over a disagreement, and
  `hardMode` not checked to be true or false.

### E5. Solver host
- The background script and the messages: board, hard mode and strategy in;
  hint, "no word", or an error out.
- **Done when:** with the real `nudge.mjs`, hints match the solver; fallback
  and impossible boards give "no word"; hard-mode hints pass rule B.

**Done in E5.** The background script answers hint requests with the real
solver. `answer()` (`src/background/solverHost.ts`) takes the finished rows,
hard mode and strategy, and returns one of four answers (`src/messages.ts`):
`hint`, `solved` (the last row is all green), `no-word` (fallback or an
impossible board: "we don't know what the word is :C"), or `error` (a row
or strategy the reader should never send).

- **Tested with the real `nudge.mjs`** in Node: four whole games played the
  way the extension will (one request per turn with every row so far) give
  exactly the native solver's moves, minimax and entropy, normal and hard;
  every hard-mode hint passes an independent rule-B checker
  (`test/reference.ts`, the rules written again for the tests).
- **A fresh solver per request, always deleted,** even when the solver
  throws (then the answer is an `error`, never a thrown exception); a
  counting stand-in proves no solver is left behind.
- `main.ts` loads the solver **once**, on the first request, by its URL in the
  extension (`import(browser.runtime.getURL('nudge.mjs'))`) instead of a fixed
  import, so esbuild doesn't try to bundle it and a test can point it at the
  real build. **A failed load** (found in the E5 review) is answered as an
  `error`, like every other problem, and forgotten, so the next request tries
  again instead of every hint failing until Firefox restarts the extension.
- Mutation checks, each caught: the solver never deleted, fallback not
  reported, an impossible board reported as an error, hard mode ignored, the
  strategy ignored, the solver loaded on every request.
- Vitest clears mock call records between tests here, so the "loaded once"
  test records calls in a plain list instead of a mock.

### E6. The shroud
- `shroud.ts`: the boxes with only the known greens that fit; reveal one
  random covered box; reveal all.
- **Done when:** tests cover a probe hint whose letters break the greens, a
  hard-mode hint, and repeated "reveal a letter" until the word shows.

**Done in E6.** `src/shroud.ts`: `shroud(hint, rows)` covers every box except
where a finished row has a green **and** the hint has the same letter there;
`revealLetter` uncovers one covered box at random (the random source is
passed in, so tests are repeatable); `revealAll` uncovers everything. All
three return a new shroud and leave the old one as it was.

- Tested on moves from the native games: in hard mode the greens fit
  (`bions` keeps the N that `crony` made green); a normal-mode probe breaks
  them (`bumph` has a U where `tacit` made the A green, so nothing shows);
  greens from several rows add up (`cover` after `rover`), yellows never
  show. The colours themselves come from the tests' own feedback rule
  (`test/reference.ts`), not from the solver.
- "Reveal a letter" pressed five times on a fully covered hint uncovers one
  new box each time, and does nothing once the word shows.
- Mutation checks, each caught: any green shown whether it fits or not,
  yellows shown, a reveal covering what was showing, a "reveal" landing on a
  box already open.

### E7. The panel on the page
- `panel.ts` and `main.ts`: draw each state (hint, "we don't know what the
  word is :C", "can't read the board" with the report link), open on page
  load and after each guess, stay quiet after a win or a loss.
- **Done when:** jsdom tests drive each state from the saved copies.

**Done in E7.** Three files. `src/content/panel.ts` only draws: one
`<nudge-panel>` element with its own shadow root, so NYT's styles and nudge's
never touch. It shows the shroud with "Reveal the hint" and "Reveal a letter",
"we don't know what the word is :C", or "nudge can't read the board" with the
report link and the reason (any other error, from the solver or from reaching
the background script, says "nudge ran into a problem." instead, with the same
link); a × closes it. All text is set as text, never as
markup. `src/content/page.ts` is the loop: it reads the board on start and on
every change to the page (a `MutationObserver` on rows and tile colours), and
only when the finished rows differ from the last ones does it read hard mode,
ask the background script, and open the panel. `src/content/main.ts` starts it
with the real page, NYT's saved data and `browser.runtime.sendMessage`.

- Tested with the saved copies, the real panel and the real solver standing
  in for the background script: the opener on an empty board, hard mode sent
  with the request, the mid-game hint, nothing asked on a won or lost board,
  the unreadable board's message with "Row 2, tile 1", and unreadable hard
  mode. Guesses are played into row 4 the way NYT does it (the tiles turn
  over in place), using the won game's own rows: `troop` brings a new hint
  and reopens a closed panel; `motor` (a win) closes it and asks nothing.
  Typing changes nothing; an answer for an older board is dropped; a board
  drawn late is waited for (5 s), and one that never comes is reported.
- Found while writing it: `:host { all: initial }` in the panel's style
  outranks the browser's own rule for `[hidden]`, so the panel would never
  hide; it now has its own `:host([hidden])` rule (pinned by a test). And
  `readBoard` now exports its "no Row elements" reason, which the loop treats
  as "not drawn yet" until the wait is over.
- In jsdom tests, Vitest can't import the solver from outside `extension/`,
  so `page.test.ts` loads it with Node's own `require`.
- Mutation checks, each caught: asking again when the finished rows haven't
  changed, drawing a late answer for an older board, missing the win or the
  loss, never hiding, losing the error text, not waiting for the board,
  never giving up waiting, ignoring or hiding unreadable hard mode, and not
  watching for new rows.
- Left for E10 in Firefox: NYT may save the new row a moment after the tiles
  turn over; if the saved settings disagree in that moment, the panel would
  say it can't read hard mode.

### E8. Toolbar button and switches
- The per-tab button behavior, the "Nudge" popup, the two switches,
  remembered.
- **Done when:** tests with a fake `browser.storage` show both switches
  survive a restart and a new strategy gives a new hint at once.

**Done in E8.** The toolbar button, the two switches, and the "Nudge" popup.

- **The button, per tab:** the manifest gives every tab the "Nudge" popup
  (`src/popup/`: the name and the "Open after each guess" switch). On
  Wordle, the page script says so on start (`wordle-page`), and the
  background script turns that tab's popup off (`action.setPopup` with an
  empty popup); a click there then reaches the background script, which tells
  the page to show or hide the panel (`toggle`). Firefox puts the popup back
  when the tab moves to another page (MDN, `action.setPopup`).
- **The switches** sit under the hint in the panel: "Open after each guess"
  (a switch) and Minimax / Entropy. `src/settings.ts` loads them from
  `browser.storage.local`, checking each value (a wrong one gives that
  switch's default). Flipping one only saves it; every Wordle tab, this one
  included, hears the change from storage and follows it, so two tabs never
  disagree.
- **"Open after each guess" off:** the hint is still worked out after each
  guess, so the button shows it at once, but the panel opens only on a click;
  problems wait for the click too. **A new strategy** asks again for the same
  board at once and redraws the panel, without opening a closed one.
- **After a win or a loss** the button shows "This game is over." (new: the
  spec says the button still shows the panel then, but not what it says).
- Tests: the switches survive a restart, with wrong saved values; the panel's
  switches show and pass on each change; the page loop with the switch off,
  with a new strategy (the empty board's hint becomes `tarse`), and with the
  other switch flipped (nothing new asked); the button on a Wordle tab and
  from no tab; the page script's hello, toggle, saving and following; the
  popup; the manifest's default popup, built by `build.mjs`.
- Mutation checks, 20, each caught: among them the switch ignored, a strategy
  change opening a closed panel or not asking again, every switch asking
  again, the popup left on Wordle, toggling on any message, no hello, a
  switch not saved, changes not followed, the popup ignoring or not saving
  its switch, and saved values not checked. One survived: the panel's toggle
  checked "on the page yet?", which can't matter (a panel not on the page is
  always hidden), so that check was deleted.
- Found in review, each fixed with a test that failed first: flipping the
  strategy after a win closed a panel the player had just opened (now only
  the winning guess closes it); and Back to a Wordle page kept in Firefox's
  back-forward cache got the popup back without the script starting again,
  so the page script now says hello again on `pageshow` from the cache.
- Left for E10 in Firefox: a click on Wordle before the page script has said
  hello (right after the page opens) shows the "Nudge" popup once; and
  whether NYT's Wordle page is kept in the back-forward cache at all.

### E9. Dark mode, like Wordle
- **First, look at the live page** with the recorder: turn on Wordle's dark
  theme, and separately put Firefox in dark mode with Wordle's own switch
  untouched. Note the colour behind the board in each, and what NYT changes
  when the theme switches (a class or style on `<html>` or `<body>`?), so the
  page script knows what to watch. The recorder also saves that colour (a
  colour value only, nothing personal; the fixture guard allows it), and a
  dark saved copy joins the others.
- `theme.ts`: `readTheme(document)` → `"light"` or `"dark"` (E-D13).
- The panel's colours become CSS variables with a light and a dark set
  (Wordle's own, spec "How the panel looks"); `panel.setTheme(theme)` picks
  one, without redrawing the hint.
- The page script reads the theme on start and whenever the watched element
  changes, sets it on the panel, and saves it to `browser.storage.local` when
  it changes. The "Nudge" popup (E8) reads the saved theme, or follows
  Firefox's setting when none is saved.
- **Done when:** jsdom tests show the light copy gives a light panel and the
  dark copy a dark one (the recorded colour put behind the board), a theme
  switch on the page changes the panel at once and asks for no new hint, the
  theme is saved once per change, and the popup shows the saved theme or
  Firefox's. In E10, both ways of going dark are checked on the live page.

### E10. In Firefox, end to end
- The manual checklist on the live page; the 1-second check (from a guess
  to the hint being ready); no network requests in Firefox's network panel;
  the packaged extension. Delete `spike/firefox/`.
- **Done when:** every checklist item passes, with the numbers recorded here.

### E11. Release (CD)
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
| NYT changes how its dark theme looks | Panel picks the wrong theme | Only the look is wrong, never the hint; no report asked. A new saved copy shows what changed (E9) |

## 7. Decided, and still to decide

Decided (draft 2): the page panel (E-D1); TypeScript, esbuild, Vitest and
jsdom; minimum Firefox 128; 100% coverage; CI/CD (E-D11, E11); the tooling
of E-D12.

Still to decide, before E11: listed on AMO (addons.mozilla.org, Mozilla's
add-ons site: public, reviewed) or unlisted (signed by AMO, shared from
GitHub Releases).
