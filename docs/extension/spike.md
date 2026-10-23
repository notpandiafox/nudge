# Spike: the solver in Firefox, and the NYT Wordle page

A throwaway Firefox extension (`spike/firefox/`), run by the developer on the
real NYT Wordle page in Firefox, to answer three questions before the
extension's spec. It only read the page; it never typed or clicked.

## 1. Does the solver load inside a Firefox extension? Yes.

- The WebAssembly module loads in an extension page (the popup) in 4–10 ms
  and plays the stored first guess (`olate`). Hints on real boards took
  2–4 ms.
- It needs the extension's content security policy to allow compiling
  WebAssembly: `"extension_pages": "script-src 'self' 'wasm-unsafe-eval'"`
  (Manifest V3).
- **The first try failed** with "call to Function() blocked by CSP": embind
  builds functions with `new Function()` by default, which extensions may not
  do. Fixed with `-sDYNAMIC_EXECUTION=0` in the WebAssembly build (plan
  §5.15); a Node test now fails if the module ever contains `new Function`
  or `eval` again.
- `nudge.mjs` finds `nudge.wasm` next to itself; no extra loading code was
  needed.

## 2. Can the board be read? Yes, row by row.

What the page has (Wordle, 2026-10):

- Each row is an element with `aria-label="Row 1"` … `"Row 6"`.
- Each tile inside it has `data-state` (`correct`, `present`, `absent`; and
  `empty` or `tbd` while not finished), its letter as text (lowercase), and a
  spoken label such as `"3rd letter, I, correct"` or
  `"1st letter, L, present in another position"`.
- Class names like `Tile-module_tile__UWEHN` end in build hashes that change
  when NYT rebuilds its site: don't use them.

**The first reader was wrong:** it took every `[data-state]` element on the
page in fives. NYT's menu buttons (`Popover…`, `data-state="closed"`) come
first, so every row was shifted by three and misread. The solver caught it:
the second row came back `ImpossibleState`. The fixed reader reads each `Row`
element on its own, and checks every tile's letter and color against its
spoken label (no mismatches on the real page). The keyboard keys also get
`data-state` once letters are used; reading by row ignores them.

So a misread shows up two ways, both usable for "nudge can't read the board,
please report it": a tile whose label disagrees with its letter or color, and
a board the solver calls `ImpossibleState`.

## 3. Where is hard mode? In the page's saved data.

- Not on the page itself (only inside NYT's settings dialog).
- `localStorage` key `games-settings-wordleV2/<account>`: the setting,
  `{"puzzleId":"settings","data":{"hardMode":true,…}}`.
- `localStorage` key `games-state-wordleV2/<account>`: today's game, with its
  own `"hardMode":true` and `"status":"IN_PROGRESS"` (or `"WIN"`).
- `<account>` is the NYT account number when signed in, or `ANON` when not.
  **Both can exist at once** (seen in the first run), so the spec must say
  which one nudge reads.
- Turning hard mode on changed both keys from `false` to `true`.
- The saved game state also holds the guesses, and possibly the answer. The
  intent says nudge only reads what's visible on screen, so it reads the
  board from the page and takes only the `hardMode` flag from saved data.

## Also seen

- Manifest V3 host permission for `nytimes.com` worked for a temporary
  add-on.
- In the spike the solver always ran in normal mode, so on a hard-mode board
  it suggested `adbot`, which breaks the greens (`_ R I _ _`) and which NYT's
  hard mode would reject. The real extension reads NYT's hard-mode setting
  (above) and passes it to the solver.
