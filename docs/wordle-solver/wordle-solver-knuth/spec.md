# Spec: Wordle solver, Knuth minimax strategy

> Derived from [intent.md](intent.md). Builds on the shared
> [Wordle solver spec](../spec.md), which defines the solver object, the
> feedback function, word lists, tie-break, hard mode, errors and latency.
> This file only covers what is specific to minimax.

## Overview

`MinimaxStrategy` implements `ScoringStrategy` using Knuth's minimax rule from
his Mastermind algorithm, adapted to Wordle: pick the guess whose worst
possible outcome leaves the fewest words.

## Definitions

- **Bucket**: for a guess, the group of candidates that would all produce the
  same feedback pattern. Every candidate falls in exactly one bucket.
- **Worst-case bucket size**: the size of the largest bucket for that guess.

## Functional requirements

- For a guess `g` and the current candidates, group the candidates by
  `computeFeedback(g, candidate)` and take the largest group's size, `W(g)`.
- `score(g) = -W(g)`, so a smaller worst case scores higher (the shared
  interface treats higher as better).
- The solver evaluates every word in the guess pool, not only the candidates.
  In normal mode that is the full guess list; in hard mode it is the allowed
  subset.
- Ties use the shared tie-break. Its first rule (prefer a candidate) is
  Knuth's original tie-break; the other two rules are this project's.
- `precomputedFirstGuess()` returns the stored minimax first guess.

## Acceptance criteria

- On a small hand-built candidate set (for example 4 to 6 words), the
  worst-case bucket size for several guesses matches values computed by
  hand.
- A test case where the best guess is **not** a candidate, to prove the
  search covers the full guess pool.
- Playing a full simulated game for every word in the answer list in
  normal mode, the solver reaches the answer within 6 guesses every time.
- The same full run in hard mode records the guess-count distribution and
  how many words, if any, take more than 6 guesses (see the shared spec's
  flagged concerns).
- The recorded results include average guesses, worst case and the full
  distribution, so they can be compared with the entropy strategy.

## Resolved open questions

All open questions from this intent are resolved in the
[shared spec](../spec.md#resolved-open-questions): tie-break, word lists,
proof on every answer, precomputed first guess, impossible board state,
hard mode and latency.

## Nice to have (not required for this phase)

- A side-by-side benchmark report against the entropy strategy. The
  acceptance criteria above already produce the numbers it needs.

## Out of scope

- Multi-step lookahead. Minimax here scores one guess ahead.
