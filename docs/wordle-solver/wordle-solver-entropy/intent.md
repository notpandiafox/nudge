# What I want: [Wordle-entropy] solver engine

The solver engine that powers the hints for Wordle. It will use the Entropy algorithm.

## Inputs / Outputs
- This Solver will take a the history of the words that the user has already put into the wordle game and use the information given by wordle, which letters are in the right position, which letters are present in the word as well as the amount of guesses so far
- This solver will return a word. 

## Definition of correct / optimal
- The best move is the word that maximizes Shannon entropy, or how best it splits the pool of remaining possible secret words into smaller, more balanced outcome groups based on the colored feedback
- We know the solver is right when, for every word in the answer list, the solver reaches that word within 6 guesses.

## Performance target
- There are no latency requirements but just make sure it is as fast as possible while stil giving the most optimal answers

## Must have
- Given the current set of possible answers, the solver picks the
  guess that maximizes expected information gain (Shannon entropy),
  not the worst case.
- The solver searches the full valid-guess list (~13,000 words), not
  just the shrinking candidate list, for the same reason as the
  minimax engine — a non-candidate guess can split the remaining
  words more evenly.
- Entropy is computed correctly: for each guess, group remaining
  candidates by feedback pattern, and score using
  Σ p·log₂(1/p) over the resulting bucket sizes (p = bucket size ÷
  total remaining candidates).
- Reuses the same feedback function as the minimax engine (not a
  reimplementation) — both solvers must agree on what a given
  guess/answer pair's feedback pattern is.
- Given a full game, the entropy solver always converges to the
  correct word within Wordle's 6-guess limit, for every word in the
  answer list.
- Deterministic output — same board state always produces the same
  suggested guess, with a defined tie-break rule for equal-entropy
  guesses.
- Independently unit-tested: the entropy calculation itself is tested
  against hand-computed examples, separate from end-to-end game tests.
- The user can pick which strategy (minimax or entropy) powers their
  hints, so entropy is user-facing, not only a benchmark. How the
  picker works is a UI question, tracked in the top-level intent.

## Nice to have
- A benchmark comparing entropy vs. minimax across the full answer
  list: average guesses used, worst-case guesses used, and full
  distribution (histogram) for each strategy.
- Two-step lookahead entropy (scoring not just immediate information
  gain, but expected information gain over the next two guesses) for
  a closer approximation to true optimal play.
- Caching/precomputing the first guess's entropy scores, since it's
  the same expensive computation every game.

## Not doing (for now)
- 

## Open questions
- Do minimax and entropy need to produce the exact same tie-break
  behavior, or can they diverge since they're answering different
  questions?
- Is "expected information gain" computed using log base 2 (bits) or
  is the actual unit irrelevant since we only compare guesses
  relative to each other, not in absolute terms?
- Should probability p be uniform across all remaining candidates
  (each equally likely to be the answer), or weighted by real-world
  word frequency (some answers are more common/likely than others)?
  This changes what "expected" actually means.
- Does the entropy solver need its own guess pool validation, or does
  it fully inherit the minimax engine's word lists as-is?