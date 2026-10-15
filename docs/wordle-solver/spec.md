# Spec: Wordle solver (shared core)

> Derived from [docs/intent.md](../intent.md),
> [wordle-solver-knuth/intent.md](wordle-solver-knuth/intent.md) and
> [wordle-solver-entropy/intent.md](wordle-solver-entropy/intent.md).
> This file covers everything both strategies share. Each strategy's own
> scoring rule is in its own spec:
> [Knuth/minimax](wordle-solver-knuth/spec.md) and
> [entropy](wordle-solver-entropy/spec.md).

## Overview

One `WordleSolver` object tracks a single Wordle game and suggests the next
guess. The scoring rule is a separate, swappable strategy object (minimax or
entropy), so the user can switch strategy mid-game without losing the game
state. The solver only ever suggests; it never types into the game.

## Definitions

- **Feedback pattern**: the five colors the game shows for one guess, one per
  letter position, each one of `Green`, `Yellow`, `Gray`.
- **Veteran word**: a guess the player has already entered, stored together
  with its feedback pattern. The list of veteran words is the game history.
- **Answer list**: the hand-picked list of words that can be the day's answer.
- **Guess list**: every word the game accepts as a guess. It includes the
  answer list.
- **Candidates**: the words from the answer list that are still consistent
  with every veteran word. A word is consistent when
  `computeFeedback(veteranWord, word)` equals that veteran word's pattern.
  In fallback mode (see "Fallback when the answer list runs out"), candidates
  come from the guess list instead.
- **Guess pool**: the words the solver is allowed to suggest. In normal mode
  it is the full guess list. In hard mode it is the subset of the guess list
  that hard mode allows (see below).
- **Tested letter**: any letter that appears in any veteran word.

## Functional requirements

### Feedback function

`computeFeedback(guess, answer)` returns the pattern the real game would show:

1. First pass: every position where the guess letter equals the answer letter
   is `Green`.
2. Second pass, left to right over the remaining positions: a guess letter is
   `Yellow` if that letter is still unused in the answer (counting only answer
   letters not already matched by a `Green` or an earlier `Yellow`),
   otherwise `Gray`.

This is the only feedback implementation in the project. Both strategies,
the candidate filter, hard mode and the tests all use it.

### Word lists

- Two static word-list files ship inside the extension: the answer list and
  the guess list. They are never fetched at runtime, from NYT or anywhere else.
- Both use the most recent version available when the lists are pinned. Each
  file records its source and the date it was retrieved, so an update is a
  deliberate, reviewable change.
- **Answer list source:** [stuartpb/wordles](https://github.com/stuartpb/wordles),
  file `wordles.json`: 2,309 hand-picked answer words, in the original
  game's daily order. Last updated February 2022. The repo has no license
  file; it describes common English word lists as commonly considered public
  domain.
- **Guess list source:** [tabatkins/wordle-list](https://github.com/tabatkins/wordle-list)
  (MIT license), file `words`: 14,855 lowercase 5-letter words, no
  duplicates, in random order. Last updated September 2022.
- The two sources were checked against each other: every one of the 2,309
  answers is in the guess list, so the solver can always suggest a word
  that might be the answer. A test keeps it that way (see acceptance
  criteria).
- stuartpb's `nonwordles.json` (10,638 words) is not used. The tabatkins
  list contains all of those words plus 1,908 guesses NYT added later.

### Solver behavior

- With no veteran words, `getBestGuess()` returns the current strategy's
  precomputed first guess (see "Precomputed first guess").
- With veteran words, `getBestGuess()` scores every word in the guess pool
  against the current candidates with the current strategy, and returns the
  best-scoring word, using the tie-break below.
- When exactly one candidate remains, `getBestGuess()` returns it.
- When the last veteran word's pattern is all `Green`, the game is solved and
  `getBestGuess()` returns that word.

### Tie-break (shared by both strategies)

When two or more guesses have the same score, pick by these rules in order:

1. Prefer a guess that is still a candidate, so a lucky guess wins outright.
   (If none of the tied guesses is a candidate, keep all of them.)
2. Among what's left, prefer the guess with the most distinct letters not yet
   tested.
3. If still tied, take the first one alphabetically.

Rule 3 means every tie is resolved, so the result is always the same.

### Hard mode

- The extension detects whether NYT's hard mode is on and passes it to the
  solver (`setHardMode`). How it detects this is an extension question,
  tracked in the top-level intent.
- In hard mode the guess pool only contains guesses that hard mode allows:
  every `Green` letter stays in its position, and every `Yellow` letter
  appears somewhere in the guess.
- Switching hard mode on or off mid-game recomputes the guess pool; the
  veteran words are kept.

### Fallback when the answer list runs out

Both lists are from 2022, and NYT now has an editor choosing answers, so a
real answer may be missing from the answer list. Without a fallback, the
solver would run out of candidates and show the error on a valid board.

- After a new veteran word is added, if no word in the answer list is still
  consistent, the solver switches to **fallback mode**: it takes the
  candidates from the guess list instead.
- Fallback mode lasts until `reset()`.
- `isUsingFallback()` reports it, so the extension can tell the user the
  answer is probably newer than the word list. What the extension shows is
  an extension question.
- The 6-guess guarantee only covers words in the answer list. Fallback games
  are measured, not guaranteed.

### Impossible board state

- After a new veteran word is added, if no candidates remain even in fallback
  mode (no word in the guess list is consistent), the board state is
  impossible. The most likely cause is a misread board.
- `addFeedbackPattern` then rejects that veteran word, leaves the solver state
  unchanged (including whether fallback mode is on), and returns
  `ImpossibleState`.
- The extension shows: "This board state is not possible. Please report this
  issue." with a link to the project's GitHub issues page, and shows no
  hint.

### Precomputed first guess

- Each strategy has its own first guess, computed ahead of time by a build
  script from the pinned word lists, and stored as data.
- It is the same in hard mode, because hard mode has nothing to restrict
  before the first guess.
- A test recomputes it from the word lists and fails if the stored value is
  out of date.

## Interfaces

Object-oriented C++, compiled to WebAssembly for the extension. This is the
public contract only; internals belong in plan.md.

```cpp
enum class Tile { Gray, Yellow, Green };
using FeedbackPattern = std::array<Tile, 5>;

struct VeteranWord {
    std::string word;
    FeedbackPattern pattern;
};

enum class Status { Ok, InvalidWord, InvalidPattern, ImpossibleState };

// Shared feedback function
FeedbackPattern computeFeedback(const std::string& guess, const std::string& answer);

class ScoringStrategy {           // MinimaxStrategy and EntropyStrategy implement this
public:
    virtual ~ScoringStrategy() = default;
    // Higher is always better.
    virtual double score(const std::string& guess,
                         const std::vector<std::string>& candidates) const = 0;
    virtual std::string precomputedFirstGuess() const = 0;
};

class WordleSolver {
public:
    WordleSolver(std::unique_ptr<ScoringStrategy> strategy, bool hardMode);

    Status addFeedbackPattern(const std::string& word, const FeedbackPattern& pattern);
    std::string getBestGuess() const;

    std::size_t getRemainingCount() const;
    const std::vector<VeteranWord>& getVeteranWords() const;
    bool isUsingFallback() const;
    void setStrategy(std::unique_ptr<ScoringStrategy> strategy);
    void setHardMode(bool on);
    void reset();
};
```

- `addFeedbackPattern` accepts any word of exactly 5 letters A–Z, even one
  that isn't in the guess list: the real game already accepted it, so the
  solver trusts it. The guess list only limits what the solver *suggests*.
- It returns `InvalidWord` for a word that isn't exactly 5 letters A–Z, and
  `InvalidPattern` for a pattern that isn't 5 values. In both cases the state
  is unchanged.
- Errors are returned as a `Status`, not thrown, because exceptions are off by
  default in Emscripten's WebAssembly builds.

## Non-functional requirements

- **Deterministic:** the same veteran words, strategy and hard-mode setting
  always produce the same suggested guess.
- **Latency:** a hint appears within 1 second of the user asking, on every
  turn, in normal and hard mode. Measured from the click to the hint being
  shown, in Firefox on the developer's laptop. The first turn is effectively
  instant because the first guess is precomputed.
- **Offline:** the solver needs no network access.

## Acceptance criteria

- `computeFeedback` matches these hand-checked cases (and each is also
  confirmed once against the real game):
  - guess `SPEED`, answer `ERASE` → Yellow, Gray, Yellow, Yellow, Gray
  - guess `SPEED`, answer `ABIDE` → Gray, Gray, Yellow, Gray, Yellow
  - guess `LEVEL`, answer `HELLO` → Yellow, Green, Gray, Gray, Yellow
  - guess `ALLEY`, answer `HELLO` → Gray, Yellow, Green, Yellow, Gray
- The tie-break has its own unit tests: one case per rule, including a case
  that only rule 3 resolves.
- The same input, run 10 times, returns the same guess every time.
- In hard mode, every suggested guess is allowed by hard mode, checked across
  full simulated games for every word in the answer list.
- Every word in the answer list is also in the guess list.
- A 5-letter word that isn't in the guess list is accepted as a veteran word
  and narrows the candidates like any other guess. A 4- or 6-letter word, or
  one with a non-letter, returns `InvalidWord`.
- A game whose answer is in the guess list but not in the answer list
  switches to fallback mode (`isUsingFallback()` is true), and the solver
  still reaches the answer.
- Feeding a pattern that no word in the guess list matches returns
  `ImpossibleState` and leaves `getRemainingCount()`, `getVeteranWords()`
  and `isUsingFallback()` unchanged.
- The stored first guess for each strategy matches a fresh recomputation.
- The latency benchmark records the slowest turn across full games; it stays
  under 1 second.

## Resolved open questions

| Question (from the intents) | Decision |
|---|---|
| Full tie-break rule | Candidate first, then most untested letters, then alphabetical |
| Guess list: match NYT exactly? | Use the most recent hand-picked answer list and guess list, pinned with source and date |
| Where the word lists come from | Answers from stuartpb/wordles, guesses from tabatkins/wordle-list |
| What if NYT uses an answer missing from the list? | Fallback mode: take candidates from the guess list before reporting an impossible board |
| Prove "within 6" on every answer, or a sample? | Every word in the answer list (normal mode) |
| First guess precomputed or hardcoded? | Precomputed by a build script, checked by a test |
| Impossible board state | Rejected with `ImpossibleState`; extension shows the report message |
| Where "report this issue" links to | The project's GitHub issues page |
| Hard mode in scope? | Yes. Extension detects it; solver only suggests allowed guesses |
| Latency target | Under 1 second per hint, every turn |
| Do the strategies share tie-break behavior? | Yes, one shared tie-break |
| Does entropy have its own word lists? | No, both strategies use the same lists |

## Carried forward (needs a decision)

- **Hard-mode rule details.** Confirm against the real game how NYT treats
  repeated letters in hard mode (for example two `Yellow` E's), before
  finalizing the guess-pool filter.

## Out of scope

- Auto-typing guesses into the game.
- Word lists other than English.
- Suggestions for any NYT game except Wordle (Strands is deferred).

## Flagged concerns

- **Hard mode can break the 6-guess guarantee.** Restricting the guess pool
  sometimes rules out the best splitting guess, and some word families (such
  as the `_IGHT` words) are known to be hard to finish in hard mode. The
  6-guess requirement therefore applies to normal mode; for hard mode the
  benchmark measures and reports how many words take more than 6 guesses.
- **Both word lists are from 2022.** Fallback mode keeps a missing answer
  from showing as an impossible board, but hints get weaker in fallback
  mode because the candidates include obscure words. If fallback mode shows
  up often, the answer list needs updating.
- **A guess NYT added after September 2022 is unknown to the solver.** The
  solver still accepts it as a veteran word when the player types it, but it
  will never suggest it.
- **The 1-second target is measured on one machine.** A slower laptop could
  exceed it. Acceptable for v1, but worth knowing.
