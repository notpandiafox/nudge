# What I want

A firefox extension that scans your NYT online game (so far just wordle and strands) and give hints to the user only when the user asks for it.

## Must have
- a drop down in the top right of the screen that lets the user interact with the UI. 
- The dropdown only opens on click, and the extension never surfaces a hint unless the user explicitly asks for one
- A solver engine per game (Wordle, Strands) that computes the objectively best next move at any point in the game
- The extension only reads board state already visible on the user's own screen — it never fetches answers from NYT's servers
- For Wordle, the user can pick which strategy (minimax or entropy) powers their hints

## Nice to have
- automatically know what NYT game is being played in the browers so the user does not have to select what they are playing. 
- nice for it to be offline

## Not doing (for now)
- Connections, Spelling Bee, or other NYT games beyong Wordle and Strands
- Any backend, server, or account system
- Auto-filling or auto-playing guesses on the user's behalf
- hint history or cross device sync stats

## Open questions
- How resilient does DOM-reading need to be to NYT UI changes — is a broken extension until a manual fix acceptable, or does it need fallback detection?
- Dies the hint ladder apply the same way to Strands as to Wordle, given structurally different puzzles?
- what happens for a puzzle you don't have a solver for, or when the extension can't confidently read the board
- do I need to recreate the games so that the solvers have something to test against?
- For Wordle the user picks which strategy (minimax or entropy) powers their hints. Where does the picker live in the dropdown, which strategy is the default, and is the choice remembered between sessions?