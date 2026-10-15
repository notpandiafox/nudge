# What I want: [Wordle-knuth] solver engine

The solver engine that powers the hints for Wordle. It will use knuth's 5 guess algorithm for mastermind but updated for solving Wordle.

## Inputs / Outputs
- This Solver will take a the history of the words that the user has already put into the wordle game and use the information given by wordle, which letters are in the right position, which letters are present in the word as well as the amount of guesses so far
- This solver will return a word. 

## Definition of correct / optimal
- The best move is the guess whose worst-case remaining group is as small as possible.
- We know the solver is right when the solver gets an input of 5 letters in the correct place as stated by the webpage. 

## Performance target
- There are no latency requirements but just make sure it is as fast as possible while stil giving the most optimal answers

## Must have
- Given the current set of possible answers, the solver picks the
  guess that minimizes the size of the largest feedback-group it
  could produce (Knuth's minimax criterion) — not average-case,
  not entropy.
- The solver searches over the full valid-guess list (~13,000 words),
  not just the shrinking list of possible answers — a guess doesn't
  have to be a candidate answer to be evaluated.
- The feedback function correctly implements Wordle's actual rules,
  including duplicate-letter cases (e.g. guessing "SPEED" against
  answer "ERASE" must produce the same pattern the real game would).
- When multiple guesses tie on worst-case bucket size, the solver
  prefers one that is still a valid possible answer (so a lucky guess
  can win immediately), per Knuth's original tie-break rule.
- Given a full game (a real answer and repeated solver-vs-itself play),
  the solver always converges to the correct word within Wordle's
  6-guess limit, for every word in the answer list — no unsolvable
  cases.
- The feedback function and the minimax scoring function are each
  independently unit-tested against known/hand-verified examples,
  not just tested end-to-end.
- The solver's output is deterministic — the same board state always
  produces the same suggested guess.
- The user can pick which strategy (minimax or entropy) powers their
  hints, so minimax is not the only user-facing engine. How the picker
  works is a UI question, tracked in the top-level intent.

## Nice to have
- A precomputed first move, caching the first guess's full result
- Knuth's algorithm compared against the entropy algorithm

## Open questions
- Full tie-break rule: "prefer a valid candidate answer" is defined,
  but what happens if multiple candidates still tie after that —
  alphabetical order? First in the word list? Does this need to be
  arbitrary-but-fixed, or should it matter which one is picked?
- Should the solver's guess pool (~13,000 words) match exactly the
  same word list the real NYT Wordle accepts, or is a close
  approximation acceptable? What happens if a word NYT accepts isn't
  in our list, or vice versa?
- Does "always solves within 6 guesses" need to be proven for every
  word in the answer list before this phase is considered done, or
  is a high-confidence sample acceptable for v1?
- Is the very first guess (before any board state exists) precomputed
  once and hardcoded, or computed fresh every time the extension
  loads? Does it matter if it's slightly stale if NYT's word list
  changes?
- How does the solver behave if it's given a board state that
  shouldn't be possible (e.g. corrupted or misread feedback from a
  DOM-reading bug) — does it error, guess something arbitrary, or
  need its own validation layer?
- Should hard-mode constraints (Wordle's optional setting where you
  must reuse revealed clues) be supported, or is that explicitly out
  of scope for v1?
- Is there a maximum acceptable latency for computing a hint, and
  does that number change between turn 1 (largest search) and later
  turns (smallest search)?