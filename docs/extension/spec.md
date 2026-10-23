# Spec: the nudge Firefox extension (Wordle)

> Derived from [docs/intent.md](../intent.md) (its must-haves and the
> "Answers so far" section) and the [spike](spike.md). The solver it uses is
> specified in [docs/wordle-solver/spec.md](../wordle-solver/spec.md) and
> built by [its plan](../wordle-solver/plan.md); this spec only says how the
> extension uses it. Strands is out of scope here (see the intent's "Later").
>
> **Draft 2.** Draft 1's suggested defaults were all confirmed.

## Overview

nudge is a Firefox extension that gives Wordle hints, only when the player
asks. It reads the board already on the player's screen, asks the solver for
the best next guess, and shows it **shrouded**: covered, until the player
reveals it, or one letter of it. It runs entirely in the browser, with no
server and no network requests.

## Definitions

- **The page:** NYT's Wordle at `https://www.nytimes.com/games/wordle/`.
- **Board:** the rows of guesses on the page. A row is **finished** when all
  five tiles have a color (Green, Yellow or Gray).
- **The panel:** nudge's hint panel (see "Where the panel appears").
- **Shrouded hint:** the hint word with its letters covered. Each covered
  letter is a blank box.
- **Known green:** a letter some finished row has as Green, at that row's
  position.
- **Unreadable:** nudge can't read the board or the hard-mode setting with
  confidence (see "Reading the board").

## Functional requirements

### Which game is open

- nudge knows the game from the tab's address. On the Wordle page it is
  active.
- On any other NYT game (for example Strands), or any other site, nudge never
  shows the panel by itself and reads nothing from the page.

### Reading the board

- nudge reads the board from the page, row by row (spike §2): each row
  element labelled `Row 1` … `Row 6`, and in it five tiles with a color
  (`data-state`) and a letter.
- Only finished rows count. A row still being typed, or empty, is ignored.
- Every tile is checked twice: its letter and color must agree with its
  spoken label (for example `"3rd letter, I, correct"`).
- The board is **unreadable** when a tile's two readings disagree, when a
  finished row doesn't have exactly five tiles, or when no `Row` elements are
  found on the Wordle page.
- nudge never reads the answer, and never takes the guesses from NYT's saved
  data: only from what is on screen (intent must-have).

### Reading hard mode

- nudge reads the player's hard-mode setting from the page's saved data
  (spike §3), and nothing else from it:
  - from today's game, `games-state-wordleV2/<entry>` → `hardMode`, using
    the entry whose number of finished rows matches the board on screen when
    there is more than one (signed in and `ANON`);
  - if no game entry matches, from `games-settings-wordleV2/<entry>` →
    `hardMode`.
- If neither can be read, hard mode is **unreadable**, handled like an
  unreadable board.
- There is no hard-mode switch in nudge.

### When the panel appears

- On the Wordle page, with the automatic panel switched on, the panel opens
  by itself **when the page opens and after each guess** (each time a row
  becomes finished).
- With it switched off, the panel opens only when the player clicks nudge's
  toolbar button.
- Once the game is won or lost, the panel doesn't open by itself again;
  clicking the button still shows it.

### What the panel shows

On the Wordle page, with a readable board:

- **The shrouded hint:** five boxes, the best next guess from the solver for
  the board, the strategy and the hard-mode setting.
  - A box shows its letter already when that letter is a known green at the
    same position in the hint word. Other boxes are covered. (The hint is
    sometimes a probe word that doesn't keep the greens, so not every known
    green fits; in hard mode they always do.)
  - Before the first guess, the hint is the stored first guess (minimax
    `olate`, entropy `tarse`), with every box covered.
- **"Reveal the hint":** uncovers every box.
- **"Reveal a letter":** uncovers one covered box, chosen at random. Each
  press uncovers one more, until the whole word shows.
- A new hint (after a guess, or a changed switch) starts covered again.
- **When no word fits** (the solver is in fallback mode, or reports
  `ImpossibleState`): the panel says "we don't know what the word is :C"
  instead of a hint.
- **When the board or hard mode is unreadable:** the panel says nudge can't
  read the board, and asks the player to report it, with a link to the
  project's GitHub issues page (`https://github.com/notpandiafox/nudge/issues`).
  It shows no hint, so it never hints from a misread board.

Anywhere else (no NYT game, or a game without a solver), clicking the
toolbar button shows only "Nudge" and the automatic-panel switch.

### Switches

- Next to each other in the panel: the **automatic panel** switch (on/off)
  and the **strategy** switch (minimax or entropy).
- Defaults: automatic panel on, strategy minimax.
- Both are remembered after Firefox closes (extension storage, on this
  computer only).
- Changing the strategy gives a new hint at once, without a page reload.

### Where the panel appears

- The intent asks for a drop-down at the top right of the screen. The
  panel is drawn on the Wordle page itself, at the top right, by nudge's page
  script, because it has to open by itself after a guess, and a toolbar
  popup is expected to open only on a click in Firefox (the plan confirms
  this against Firefox's current rules). Clicking the toolbar button shows or
  hides the same panel.
- The panel never covers the board or the keyboard, and can be closed.

## Interfaces

- **Manifest V3** Firefox extension, built from TypeScript.
- **The solver:** `nudge.mjs` + `nudge.wasm` from the solver's WebAssembly
  build (solver plan §5.15), used through its JavaScript `Solver` class:
  `new Solver(hardMode)`, `setStrategy(name)`, `addFeedbackPattern(word,
  pattern)`, `getBestGuess()`, `isUsingFallback()`, `reset()`, `delete()`.
  It follows the solver's JavaScript contract: real strings and booleans,
  and one `delete()` per `new Solver`.
- **Content security policy:** `"extension_pages": "script-src 'self'
  'wasm-unsafe-eval'"`, so the extension may compile WebAssembly (spike §1).
- **Permissions:** only the Wordle page (`https://www.nytimes.com/games/wordle/*`)
  and extension storage. No other site, no network access.

## Non-functional requirements

- **Latency:** the hint is ready within 1 second of a guess being finished,
  on the developer's laptop in Firefox (the solver's share is 500 ms; the
  spike measured hints of 2–4 ms on real boards).
- **Private and offline:** no network requests of any kind; nothing leaves
  the computer. Works offline once the page is loaded (intent nice-to-have).
- **Read-only:** nudge never types, clicks, or changes anything on NYT's
  page or in its saved data (intent: no auto-playing).
- **Accessible:** the panel's buttons and switches work with the keyboard,
  and the shroud tells screen readers how many letters are covered.

## Acceptance criteria

- On saved copies of NYT's page (see "Tests"), nudge reads every finished row
  exactly: empty board, mid-game, a won game, hard mode on and off, signed in
  and signed out (both saved entries present).
- A copy where a tile's label disagrees with its letter shows "can't read the
  board" with the report link, and no hint.
- The shrouded hint shows exactly the known greens that fit the hint word,
  and nothing else; "Reveal a letter" uncovers one new box per press until
  the word shows; "Reveal the hint" uncovers all.
- A board with no possible answer, and a fallback board, both show "we don't
  know what the word is :C".
- In hard mode, every hint is allowed by hard mode (checked with the
  solver's independent rule-B checker).
- The automatic panel opens on page load and after each guess when on, and
  only on a toolbar click when off. Both switches survive a Firefox restart.
- On a non-Wordle NYT page, the button shows only "Nudge" and the switch.
- No network request is made by the extension (checked in the tests).

## Tests

- **Saved copies of NYT's page** (intent): the Wordle page's board saved at
  known states, plus copies of its saved data, kept in the repository. The
  tests run against them, never against the live site.
- When NYT changes its page, a new saved copy shows what changed, and is
  added once the reader handles it.

## Resolved open questions

| Question (from the intent) | Decision |
|---|---|
| How resilient must reading be to NYT changes? | Notice it, say "can't read the board", ask the player to report it |
| Unreadable board, or a game without a solver? | Unreadable: the message above. No solver: the panel never appears |
| Recreate the games for tests? | Yes, saved copies of NYT's page |
| Strategy picker: where, default, remembered? | A switch next to the automatic-panel switch; minimax; remembered |
| How is hard mode detected, and if it can't be? | From NYT's saved setting (spike); unreadable is handled like an unreadable board |
| Where is the drop-down? | A panel drawn on the Wordle page, top right; the toolbar button shows or hides it |
| Is the automatic panel on by default? | Yes |
| After a win or a loss? | The panel stays quiet; the toolbar button still shows it |

## Out of scope

- Strands and every other NYT game (the intent's "Later" and "Not doing").
- Any server, account, sync or hint history.
- Typing or playing guesses for the player.
- Other browsers than Firefox.
