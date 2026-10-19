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
- **Letter case**: all words are handled in lowercase. Every public function
  that takes a word accepts upper or lower case and converts it to lowercase
  first. Every word the solver returns is lowercase.

## Functional requirements

### Feedback function

`computeFeedback(guess, answer)` returns the pattern the real game would show:

1. First pass: every position where the guess letter equals the answer letter
   is `Green`.
2. Second pass, left to right over the remaining positions: a guess letter is
   `Yellow` if that letter is still unused in the answer (counting only answer
   letters not already matched by a `Green` or an earlier `Yellow`),
   otherwise `Gray`.

This is the only feedback implementation in the shipped code. Both
strategies, the candidate filter and hard mode use it. The tests also keep a
separate, deliberately simple version, used only to check this one.

### Word lists

- The answer list and the guess list are compiled into the solver from two
  files in `data/`. They are never fetched at runtime, from NYT or anywhere
  else.
- Both use the most recent version available when the lists are pinned. Each
  file records its source and the date it was retrieved, so an update is a
  deliberate, reviewable change.
- **Answer list source:** [stuartpb/wordles](https://github.com/stuartpb/wordles),
  file `wordles.json`: 2,309 hand-picked answer words, in the original
  game's daily order (the solver stores them sorted; the order isn't used).
  Last updated February 2022. The repo has no license
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

- With no veteran words, `getBestGuess()` returns the precomputed first guess
  for the current strategy (see "Precomputed first guess").
- With veteran words, `getBestGuess()` scores every word in the guess pool
  against the current candidates with the current strategy, and returns the
  best-scoring word, using the tie-break below.
- When exactly one candidate remains, `getBestGuess()` returns it.
- **An all-`Green` pattern always means solved.** `addFeedbackPattern`
  accepts it for any valid 5-letter word, even one in neither word list,
  without filtering candidates, entering fallback mode, or returning
  `ImpossibleState`. From then on `getBestGuess()` returns that word.
- **After the game is solved**, `addFeedbackPattern` returns `GameOver` and
  changes nothing, since the real game accepts no more guesses.

### Starting a new game

- `reset()` clears the game: the veteran words, and with them fallback mode
  and the solved state. It keeps the strategy and the hard-mode setting: the
  strategy is the user's choice and hard mode is the game's setting, so
  neither belongs to one game.
- The solver never resets itself. The extension calls `reset()` only when the
  Wordle board on the page is empty, which is when a new game starts.

### Tie-break (shared by both strategies)

Two guesses are **tied** when both scores are within `1e-9` of the best score.
The same constant is used for both strategies; minimax scores are whole
numbers, so for minimax this means exactly equal. Among the tied guesses,
pick by these rules in order:

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
  - every `Green` letter stays in its position;
  - **repeated letters are counted:** for each earlier guess, count its
    `Green` plus `Yellow` tiles of each letter. The new guess must contain at
    least that many copies of the letter, using the highest count from any
    one earlier guess. For example, after `SPEED` gets S, E and E lit up, the
    next guess needs an S and two E's: `EASEL` is allowed, `SENOR` is not;
  - a `Yellow` letter may be anywhere in the guess, including the same
    position;
  - `Gray` tiles add no requirement.
- This rule is deliberately at least as strict as NYT's. If NYT only checks
  that each letter appears once, the solver skips a few guesses NYT would
  allow, but never suggests one NYT rejects.
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

- Each strategy has its own first guess, stored together with the word lists
  it was computed from.
- It is computed ahead of time by a tool you run by hand (not during the
  build, because the WebAssembly build can't run a native program), and the
  result is committed and compiled in.
- For word lists without stored first guesses (for example, small lists in
  tests), the solver computes the first guess when it's needed.
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

enum class Status { Ok, InvalidWord, InvalidPattern, ImpossibleState,
                    UnknownStrategy, GameOver };

// Shared feedback function
FeedbackPattern computeFeedback(const std::string& guess, const std::string& answer);

// One counter per possible feedback pattern (3^5 = 243): how many candidates
// would produce that pattern for a given guess.
using BucketCounts = std::array<std::uint32_t, 243>;

class ScoringStrategy {           // MinimaxStrategy and EntropyStrategy implement this
public:
    virtual ~ScoringStrategy();
    // Scores one guess from its bucket counts; total = number of candidates.
    // Higher is always better.
    virtual double score(const BucketCounts& counts, std::size_t total) const = 0;
    virtual std::string_view name() const = 0;   // "minimax" or "entropy"
};

// Answer list, guess list, and the stored first guess for each strategy.
class WordLists {
public:
    static const WordLists& builtin();           // the compiled-in lists
    // For tests: sorts and checks the words; no stored first guesses. Nothing if
    // a word isn't 5 letters, a list repeats a word, an answer isn't a guess,
    // or there are no answers.
    static std::optional<WordLists> fromWords(const std::vector<std::string_view>& answers,
                                              const std::vector<std::string_view>& guesses);
    const std::vector<Word>& guesses() const;    // sorted alphabetically
    const std::vector<Word>& answers() const;    // sorted alphabetically; all also guesses
};

class WordleSolver {
public:
    WordleSolver(std::unique_ptr<ScoringStrategy> strategy, bool hardMode,
                 WordLists lists = WordLists::builtin());  // copied

    Status addFeedbackPattern(const std::string& word, const FeedbackPattern& pattern);
    std::string getBestGuess() const;

    std::size_t getRemainingCount() const;
    const std::vector<VeteranWord>& getVeteranWords() const;
    bool isUsingFallback() const;
    Status setStrategy(std::unique_ptr<ScoringStrategy> strategy);
    Status setStrategy(std::string_view name);   // "minimax" or "entropy"
    void setHardMode(bool on);
    void reset();
};
```

- `addFeedbackPattern` accepts any word of exactly 5 letters A–Z, even one
  that isn't in the guess list: the real game already accepted it, so the
  solver trusts it. The guess list only limits what the solver *suggests*.
- It returns `InvalidWord` for a word that isn't exactly 5 letters A–Z,
  `InvalidPattern` for a pattern containing a value other than the three
  colors (or, from JavaScript, pattern text that isn't 5 of `G`, `Y`, `X`),
  and `GameOver` once the game is solved. In each case the state is
  unchanged.
- `setStrategy` returns `UnknownStrategy` for an empty strategy pointer or a
  name it doesn't know (names are lowercase: `"minimax"`, `"entropy"`). The
  current strategy and the game state are unchanged.
- The constructor requires a strategy. Constructing without one is a
  programming error, caught by an assertion in debug builds. The JavaScript
  wrapper always constructs with the default strategy (minimax, until the
  top-level intent's open question about the default is decided), then calls
  `setStrategy(name)`.
- Errors are returned as a `Status`, not thrown, because exceptions are off by
  default in Emscripten's WebAssembly builds. That covers every *value* a
  caller can send: any text as a word or a pattern, and any strategy name.
- **Mistakes in the calling code are not statuses** (decided in the T12
  review). From JavaScript, an argument of the wrong type (`undefined`,
  `null`, a number, a missing argument) or a call on a solver after its
  `delete()` throws a `BindingError` before the solver sees it, and the
  solver is unchanged. The extension's own types (TypeScript) are what
  prevent these. The hard-mode flag must be a real boolean: JavaScript's
  rules make any non-empty string true, so `setHardMode("false")` turns hard
  mode on.

## Non-functional requirements

- **Deterministic:** the same veteran words, strategy and hard-mode setting
  always produce the same suggested guess.
- **Latency:** a hint appears within 1 second of the user asking, on every
  turn, in normal and hard mode. Measured from the click to the hint being
  shown, in Firefox on the developer's laptop. The first turn is effectively
  instant because the first guess is precomputed. The solver's share of that
  second is 500 ms (computing the hint); the rest is for the extension
  (reading the board, sending it to the solver, showing the hint).
- **Offline:** the solver needs no network access.

## Acceptance criteria

- `computeFeedback` matches these hand-checked cases (the rule itself is
  checked once against the original game's code, since NYT's daily answer
  can't be chosen):
  - guess `SPEED`, answer `ERASE` → Yellow, Gray, Yellow, Yellow, Gray
  - guess `SPEED`, answer `ABIDE` → Gray, Gray, Yellow, Gray, Yellow
  - guess `LEVEL`, answer `HELLO` → Yellow, Green, Gray, Gray, Yellow
  - guess `ALLEY`, answer `HELLO` → Gray, Yellow, Green, Yellow, Gray
- The tie-break has its own unit tests: one case per rule, including a case
  that only rule 3 resolves.
- The same input, run 10 times, returns the same guess every time.
- In hard mode, every suggested guess is allowed by hard mode, checked across
  full simulated games for every word in the answer list.
- Hard mode with repeated letters: after `SPEED` against `ERASE`, `EASEL` is
  allowed and `SENOR` is not; after `SPEED` against `ABIDE` (one E Yellow,
  one Gray, D Yellow), a guess with one E and a D is allowed.
- Every word in the answer list is also in the guess list.
- An all-`Green` pattern for a word in neither list returns `Ok`, and
  `getBestGuess()` then returns that word.
- After the game is solved, `addFeedbackPattern` returns `GameOver` and
  leaves the veteran words unchanged.
- `reset()` clears the veteran words, fallback mode and the solved state, and
  keeps the strategy and the hard-mode setting.
- `"CRANE"`, `"Crane"` and `"crane"` are all accepted and stored as
  `"crane"`.
- `setStrategy` with an empty pointer, or with `"knuth"` or `"Minimax "`,
  returns `UnknownStrategy` and changes nothing; `"minimax"` and `"entropy"`
  return `Ok`.
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
- The latency benchmark records the slowest turn, including the worst
  possible first guess and the worst early fallback position; computing the
  hint stays under 500 ms on the developer's laptop, natively and in
  WebAssembly.

## Resolved open questions

| Question (from the intents) | Decision |
|---|---|
| Full tie-break rule | Candidate first, then most untested letters, then alphabetical |
| Guess list: match NYT exactly? | Use the most recent hand-picked answer list and guess list, pinned with source and date |
| Where the word lists come from | Answers from stuartpb/wordles, guesses from tabatkins/wordle-list |
| What if NYT uses an answer missing from the list? | Fallback mode: take candidates from the guess list before reporting an impossible board |
| Prove "within 6" on every answer, or a sample? | Every word in the answer list (normal mode) |
| First guess precomputed or hardcoded? | Precomputed by a tool run by hand, stored with the word lists, checked by a test |
| Impossible board state | Rejected with `ImpossibleState`; extension shows the report message |
| Where "report this issue" links to | The project's GitHub issues page |
| A won game with a word in neither list | All-green always means solved, never `ImpossibleState` |
| Upper or lower case input | Either is accepted and converted to lowercase; output is lowercase |
| Unknown or empty strategy | `setStrategy` returns `UnknownStrategy` and keeps the current strategy |
| What counts as a tie | Within `1e-9` of the best score, one constant for both strategies |
| Guesses after the game is won | `GameOver`, nothing changes |
| When a new game starts | The extension calls `reset()` only when the Wordle board is empty; `reset()` keeps the strategy and hard-mode setting |
| How strategies score | From the bucket counts the solver builds (`score(counts, total)`) |
| Where the first guesses live | With the word lists; computed when not stored |
| The answer list's daily order | Not used; the lists are stored sorted |
| Hard mode in scope? | Yes. Extension detects it; solver only suggests allowed guesses |
| Hard mode with repeated letters | Counted: at least as many copies as were lit up Green or Yellow in one earlier guess (at least as strict as NYT) |
| Latency target | Under 1 second per hint, every turn |
| Do the strategies share tie-break behavior? | Yes, one shared tie-break |
| Does entropy have its own word lists? | No, both strategies use the same lists |

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
- **Fallback can start late (accepted for v1).** If the real answer is
  missing from the answer list but other answer-list words still fit the
  clues, the solver keeps narrowing those words, and can even present a
  wrong one as the last candidate. Fallback only starts once that word is
  ruled out. Every fix makes normal hints worse, so this is documented and
  pinned by a test instead.
