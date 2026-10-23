# What I want

A firefox extension that scans your NYT online game (Wordle first, Strands later) and give hints to the user only when the user asks for it.

## Must have
- a drop down in the top right of the screen that lets the user interact with the UI. 
- The dropdown only opens on click, and the extension never surfaces a hint unless the user explicitly asks for one
- A solver engine for Wordle that computes the objectively best next move at any point in the game
- The extension only reads board state already visible on the user's own screen — it never fetches answers from NYT's servers
- For Wordle, the user can pick which strategy (minimax or entropy) powers their hints
- The extension detects whether the player has NYT's hard mode turned on, and the Wordle solver then only suggests guesses that hard mode allows

## Nice to have
- automatically know what NYT game is being played in the browers so the user does not have to select what they are playing. 
- nice for it to be offline

## Later (deferred)
- A Strands solver engine and Strands hints. Gets its own intent.md and spec.md when work on it starts.
- Open question to answer then: does the hint ladder apply the same way to Strands as to Wordle, given structurally different puzzles?

## Not doing (for now)
- Connections, Spelling Bee, or other NYT games beyong Wordle and Strands
- Any backend, server, or account system
- Auto-filling or auto-playing guesses on the user's behalf
- hint history or cross device sync stats

## Answers so far (to be turned into the extension spec)
- **Which game:** the extension knows by itself which NYT game is open. With no solver for that game, the user doesn't see the popup at all.
- **The popup:** with a solved game open, the popup opens by itself with a *shrouded* hint: the hint is covered until the user asks, so nothing is shown unasked. It has two buttons, "Reveal the hint" and "Reveal a letter" (a random one). The letters already known (green) show through the shroud, but only where they fit the hint word: the best guess is sometimes a probe word that doesn't keep the greens in place (always fits in hard mode).
- **When it opens by itself:** when the Wordle page opens and after each guess.
- **Reveal a letter:** each press reveals one more letter (a random one still covered), until the whole word shows.
- **Switches:** next to each other, an on/off switch for the automatic popup (when it's off, clicking the extension's button opens the same popup), and the strategy switch (minimax or entropy). Minimax is the default. Both are remembered after Firefox closes.
- **Which saved entry:** when NYT keeps both a signed-in and a signed-out (`ANON`) game, nudge reads the one whose game matches the board on screen.
- **No NYT game open:** clicking the extension's button shows only "Nudge" and the on/off switch for the automatic popup.
- **No word fits** (the answer isn't in the answer list, or the board is impossible): the popup says "we don't know what the word is :C".
- **Hard mode:** read from the setting the player checked in NYT's settings, which the spike showed nudge can read from the page's saved data (docs/extension/spike.md). There's no hard-mode switch in nudge's popup. If the setting can't be read, nudge treats it like a board it can't read: says so and asks the user to report it.
- **When NYT's page changes** and the board can't be read: nudge notices, says it can't read the board, and asks the user to report it.
- **Tests:** the games get recreated as saved copies of NYT's page, so the extension can be tested without the live site.

## Open questions
- How resilient does DOM-reading need to be to NYT UI changes — is a broken extension until a manual fix acceptable, or does it need fallback detection?
- what happens for a puzzle you don't have a solver for, or when the extension can't confidently read the board
- do I need to recreate the games so that the solvers have something to test against?
- For Wordle the user picks which strategy (minimax or entropy) powers their hints. Where does the picker live in the dropdown, which strategy is the default, and is the choice remembered between sessions?
- How does the extension detect NYT's hard mode setting, and what happens if it can't tell?
