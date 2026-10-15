# Spec: Wordle solver, entropy strategy

> Derived from [intent.md](intent.md). Builds on the shared
> [Wordle solver spec](../spec.md), which defines the solver object, the
> feedback function, word lists, tie-break, hard mode, errors and latency.
> This file only covers what is specific to entropy.

## Overview

`EntropyStrategy` implements `ScoringStrategy` by picking the guess that is
expected to reveal the most information: the one that splits the candidates
into the most, and most even, groups.

## Definitions

- **Bucket**: for a guess, the group of candidates that would all produce the
  same feedback pattern.
- **Probability of a bucket**: with `N` candidates and a bucket of size `n`,
  `p = n / N`. Every candidate is treated as equally likely to be the answer
  (uniform probability).

## Functional requirements

- For a guess `g`, group the candidates by `computeFeedback(g, candidate)`,
  and compute `score(g) = Σ p · log₂(1 / p)` over the buckets.
- A higher score is better, matching the shared interface.
- Uses log base 2, so scores are in bits.
- The solver evaluates every word in the guess pool, not only the candidates.
- **Ties:** two guesses are tied when their scores differ by less than
  `1e-9`. Floating-point rounding can make mathematically equal scores
  differ slightly; without a tolerance, rounding would decide the tie, not
  the shared tie-break.
- `precomputedFirstGuess()` returns the stored entropy first guess.

## Acceptance criteria

- On a small hand-built candidate set, the entropy score for several guesses
  matches values computed by hand. One case: a guess that splits 4 candidates
  into 4 buckets of 1 scores exactly 2 bits, and a guess that leaves all 4 in
  one bucket scores 0.
- A test case where the best guess is **not** a candidate.
- A test case where two guesses tie within the tolerance, resolved by the
  shared tie-break.
- Playing a full simulated game for every word in the answer list in
  normal mode, the solver reaches the answer within 6 guesses every time.
- The same full run in hard mode records the guess-count distribution and
  how many words, if any, take more than 6 guesses.
- The recorded results include average guesses, worst case and the full
  distribution, in the same format as the minimax strategy.

## Resolved open questions

| Question (from the intent) | Decision |
|---|---|
| Same tie-break as minimax? | Yes, the shared tie-break |
| Log base 2, or does the unit not matter? | Log base 2 |
| Uniform or frequency-weighted probability? | Uniform |
| Own word lists, or the same as minimax? | The same shared lists |

The remaining shared questions (word lists, proof on every answer, first
guess, impossible state, hard mode, latency) are resolved in the
[shared spec](../spec.md#resolved-open-questions).

## Nice to have (not required for this phase)

- A benchmark report comparing entropy with minimax.
- Two-step lookahead entropy.
- Frequency-weighted probabilities, as a later experiment.

## Out of scope

- Hard-mode-specific scoring. Hard mode only changes the guess pool; the
  scoring formula stays the same.
