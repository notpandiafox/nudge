# Plan: Wordle solver (draft 2)

> Implements the three accepted specs: the [shared spec](spec.md), the
> [Knuth/minimax spec](wordle-solver-knuth/spec.md) and the
> [entropy spec](wordle-solver-entropy/spec.md).
>
> The specs say **what** the solver must do. This plan says **how** it will
> be built, and explains **why** each choice was made, so the reasoning can
> be reviewed and not just the result.
>
> Status: draft 2, approved. Section 11 lists the spec changes it needed
> (all now written into the specs) and what is still open.

---

## 0. What changed since draft 1

Draft 1 was reviewed adversarially by three models (Opus, Sonnet, Haiku).
Draft 2 applies everything from that review marked "act on", plus four
decisions you made.

**Your decisions (already written into the specs):**
- An all-green pattern always means the game is solved.
- Words are accepted in any case and stored lowercase.
- An unknown or empty strategy returns `UnknownStrategy`.
- "Tied" means within `1e-9` of the best score, one constant for both strategies.

**Design changes:**
- **Game state is recomputed, not updated piece by piece** (§5.5). The solver
  stores only the veteran words, the hard-mode flag and the strategy.
  Candidates, fallback, tested letters and the guess pool are rebuilt from
  them after every change.
- **All input checking lives in the C++ core** (§5.3). Converting text to
  patterns, strategy names to strategies, and statuses to text is no longer
  in the JavaScript wrapper, so coverage measures it.
- **One internal "rank all guesses" function** used by the solver, the tests
  and the precompute tool (§5.6), and the tie-break is a separate pure
  function (§5.7).
- **First guesses are stored with the word lists** (§5.11), so tiny test lists
  work correctly.
- **The is-answer flag is removed**; tie-break rule 1 uses real candidate
  membership (§5.1).
- **Exact speed shortcuts** that work for both strategies replace draft 1's
  fix list (§5.8, §5.9).
- **Word lists are written straight into include files** by the import
  script. The build-time Python generator is gone (§5.10).

**Order changes:**
- **A speed and WebAssembly spike is now task T4**, right after the feedback
  function, instead of finding out at the end (§7).
- Fallback, impossible state and all-green handling are built together with
  the solver state in T5, not added later.

**Testing changes:**
- **Independent checkers:** the full-game tests use a separate, simple
  feedback function, and hard mode is checked by a separate, simple rule
  (§8.1).
- **The full-game tests walk the decision tree** instead of caching results,
  and determinism is tested on fresh solvers (§8.2, §8.3).
- **Native and WebAssembly are compared on the whole tree**, generated fresh in
  CI (§8.4).
- **Timing is reported in CI and enforced locally** (§8.5).
- **Missing tests added:** switching strategy mid-game, turning hard mode off,
  `reset()`, the first guess in hard mode, one candidate left, all-green,
  duplicate guesses, letter case, unknown strategy.
- **Coverage gate details fixed** (§8.6), and a small mutation check added
  (§8.7).

**Repo and CI changes:**
- `.gitattributes` forces LF line endings on data files. Git on your machine
  is set to convert to Windows line endings, which would break the word
  lists.
- A Clang build is added to CI, so "builds with a second compiler" is now
  true.
- Node 24 (an active LTS release), since Node 20 is past end of life.
- `data/NOTICE` records the word lists' sources and licenses.
- Commands are single-line, so they work in PowerShell.

**Corrected claims:**
- The first-guess tool's real reason (§5.11).
- The branch-coverage reasoning (§8.6).
- Risks R1 and R3 no longer propose things the specs rule out (§10).

---

## 1. Scope

**This plan covers:** the C++ solver library (feedback function, word lists,
`WordleSolver`, both strategies, hard mode, fallback mode, errors), its
tests, its benchmark, the word-list import script, CI, and a WebAssembly
build that JavaScript can call.

**This plan stops at:** "a WebAssembly module that a JavaScript test can
load, create a solver in, feed guesses to, and get hints from."

**Not in this plan:** anything inside the Firefox extension (the dropdown,
reading the NYT board, detecting hard mode, the strategy picker, displaying
errors). The extension gets its own intent, spec and plan. Section 12 lists
what this plan hands over to it.

*Why stop at the WebAssembly boundary:* the solver can be built and fully
proven on your own machine without a browser. Everything the extension
needs from it is fixed by the spec's interface. Stopping here gives a
finished, testable piece of work before any UI exists.

---

## 2. Summary of decisions

| # | Decision | Short reason |
|---|---|---|
| D1 | C++20, CMake, GoogleTest | Modern standard both GCC 12 and Emscripten support; one build system for native and WebAssembly |
| D2 | Words in the guess list are 16-bit indices into the sorted list | Small and fast, and "alphabetical" becomes "lowest index" |
| D3 | A feedback pattern is stored internally as one number 0–242 | Grouping candidates becomes counting into a 243-slot array |
| D4 | The solver builds the bucket counts; strategies only score the counts | The expensive part is written once; strategies are tiny and easy to test |
| D5 | The solver stores only veteran words, hard-mode flag and strategy; everything else is recomputed | Nothing can go out of sync, and "unchanged on error" is automatic |
| D6 | All text input is checked and converted in the C++ core | One boundary, fully covered by tests |
| D7 | Feedback computed on demand, no table; exact shortcuts for both strategies | A full table is ~34 MB; the spike in T4 decides if more is needed |
| D8 | Word lists and first guesses are compiled in from include files | No file loading in the browser; native and WebAssembly use identical data |
| D9 | The tie-break is a separate pure function over "everything within `1e-9` of the best" | Avoids a sorting bug with tolerances, and is easy to test directly |
| D10 | Full-game tests walk the decision tree, with independent checkers | Every position computed once; tests can catch bugs in the code they check |
| D11 | Sanitizers, Clang, lint and WebAssembly run in CI on Linux | AddressSanitizer isn't available with MinGW on Windows |
| D12 | Solver latency budget: 500 ms of the 1-second total | Leaves the other half for the extension |
| D13 | 100% line and function coverage, enforced in CI, from the fast tests alone | Every line written is run by a test; a drop fails the build |

Each one is explained in the sections below.

---

## 3. Repo layout

```
nudge/
├── CMakeLists.txt                  top-level build
├── .gitattributes                  LF line endings for data and source files
├── .clang-format, .clang-tidy
├── solver/
│   ├── CMakeLists.txt
│   ├── include/nudge/              public headers (the spec's interface)
│   │   ├── feedback.hpp            Tile, FeedbackPattern, computeFeedback
│   │   ├── status.hpp              Status enum
│   │   ├── word_lists.hpp          WordLists (answers, guesses, first guesses)
│   │   ├── scoring_strategy.hpp    ScoringStrategy base class, BucketCounts
│   │   ├── minimax_strategy.hpp
│   │   ├── entropy_strategy.hpp
│   │   └── wordle_solver.hpp       WordleSolver
│   ├── src/
│   │   ├── word.cpp                Word type: checks and converts input words
│   │   ├── text_format.cpp         pattern text, status text, strategy names
│   │   ├── feedback.cpp
│   │   ├── hard_mode.cpp           rule B: constraints from the veteran words, legality check
│   │   ├── ranking.cpp             scoring loop, tie-break
│   │   ├── minimax_strategy.cpp, entropy_strategy.cpp
│   │   ├── word_lists.cpp          compiles in data/*.inc
│   │   └── wordle_solver.cpp       the game state (derive: candidates, fallback)
│   ├── tests/
│   │   ├── reference/              independent checkers: simple feedback, simple hard-mode rule
│   │   ├── tree_walk.hpp           plays every game as a decision tree
│   │   └── *_test.cpp              fast and slow tests
│   ├── bench/                      nudge_bench: statistics, worst cases, timing
│   ├── tools/                      nudge_precompute (first guesses), nudge_tree (tree dump)
│   └── wasm/                       bindings.cpp (registration only) + Node tests
├── data/
│   ├── answers.inc                 2,309 words, from stuartpb/wordles
│   ├── guesses.inc                 14,855 words, from tabatkins/wordle-list
│   ├── first_guesses.inc           precomputed first guess per strategy
│   └── NOTICE                      sources and licenses of the word lists
├── scripts/
│   ├── import_word_lists.py        downloads, checks and writes data/*.inc
│   ├── tests/                      pytest tests for the script
│   └── .coveragerc                 what Python coverage measures
├── docs/                           intents, specs, this plan
└── .github/workflows/ci.yml
```

*Why `solver/` is its own folder:* the extension will live next to it in
`extension/` with a different toolchain (TypeScript, npm). Keeping them apart
means each has its own build files.

*Why `include/nudge/`:* the public headers are the spec's interface. A
separate folder makes it obvious which files are the contract and which are
internal and can change freely. Code includes them as
`#include "nudge/wordle_solver.hpp"`, which avoids name clashes.

*Why `tests/reference/`:* these are the deliberately simple, independent
versions of the rules that tests check the real code against (§8.1). Keeping
them in their own folder makes it clear they must never be "optimized" or
shared with the production code.

*Why `.gitattributes`:* Git on your machine converts line endings to Windows
style (CRLF) when it checks files out. A word-list line would then read
`"crane",\r`, and the CR can break parsing or comparisons. Forcing LF for
`data/` and source files makes every machine see the same bytes.

---

## 4. Toolchain and commands

### 4.1 Tools

| Tool | Version | Used for | Already on your machine? |
|---|---|---|---|
| GCC (MinGW) | 12.2 | Native build on Windows | Yes |
| CMake | 3.20 or newer | Build system | Yes, bundled with CLion |
| Ninja | recent | Fast build tool CMake drives | Install, or use CLion's |
| GoogleTest | pinned release tag | Unit tests | Downloaded by CMake (`FetchContent`) |
| Python | 3.x | The import script and its tests | Yes, 3.14 |
| pytest, pytest-cov | pinned | Tests and coverage for the script | Install with `pip` |
| Node.js | 24 (active LTS) | Running the WebAssembly tests | **Upgrade needed** (you have 20.17, now past end of life) |
| Emscripten (emsdk) | pinned version | WebAssembly build | **Install needed** (task T4) |
| gcovr | pinned, 7.x or newer | C++ coverage and the 100% gate | Needed in CI; optional locally |
| clang-format, clang-tidy | pinned | Formatting and lint | Needed in CI; optional locally |

*Why C++20:* `std::popcount` (counts set bits, used by the tie-break) and
`std::span` (a cheap read-only view of a list, used to pass veteran words
and candidates around without copying). GCC 12 and Emscripten both support
C++20 fully.

*Why GoogleTest through `FetchContent`:* CMake downloads one fixed version, so
every machine, including CI, uses exactly the same version and nobody
installs it by hand.

*Why no Google Benchmark:* it's designed for timing tiny functions in a loop.
This project needs full-game statistics, guess-count distributions and the
slowest single turn, which a small custom program measures directly.

*Why pin gcovr to 7.x or newer:* the function-coverage gate
(`--fail-under-function`) only exists in newer gcovr versions. T1 confirms the
pinned version has it before anything relies on it.

### 4.2 Commands

These go into `CLAUDE.md` in task T1, merged into the existing file below the
graphify section, so Claude can run its own feedback loop after every change.
Each command is one line, so it works in PowerShell as well as Bash.

```bash
# Configure and build (Release: the full-game tests are too slow in Debug)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build

# Fast tests: after every change (seconds)
ctest --test-dir build -L fast --output-on-failure

# Slow tests: every game, both strategies, both modes (minutes)
ctest --test-dir build -L slow --output-on-failure

# Coverage: separate Debug build, fast tests only, fails below 100%
cmake -S . -B build-cov -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNUDGE_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov -L fast
gcovr -r . --object-directory build-cov --filter solver/src/ --filter solver/include/ --fail-under-line 100 --fail-under-function 100 --exclude-throw-branches --exclude-unreachable-branches --exclude-noncode-lines --txt --delete

# Python script tests with coverage
pytest scripts/tests --cov=scripts --cov-config=scripts/.coveragerc --cov-fail-under=100

# Benchmark
./build/solver/bench/nudge_bench

# WebAssembly build (emsdk activated) and its tests
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm
node solver/wasm/test/run.mjs build-wasm
```

*Why "fast" and "slow" labels:* the feedback loop only works if the tests are
quick enough to run after every edit. The full-game tests take minutes, so
they run before a commit and in CI.

*Why `CMAKE_EXPORT_COMPILE_COMMANDS`:* clang-tidy needs to know the exact
flags each file is compiled with. This writes that list for it.

*Why `emcmake` builds only the library:* when CMake sees it's building with
Emscripten, the option `NUDGE_BUILD_TESTS` defaults to off. Otherwise
`emcmake` would try to compile GoogleTest, the tests and the native tools
for WebAssembly, which isn't wanted and would likely fail.

---

## 5. Internal design

### 5.1 Words and letters

*As built (T2–T5); this replaced an earlier design that stored words as
16-bit positions in the guess list. Nothing turned out to need the positions,
and every user converted them back to words.*

- **A word is a `Word`:** its five letters as numbers 0–25 (`a` = 0), 5 bytes.
  `Word::parse` is the only way to make one (§5.3).
- **Both lists are sorted vectors of `Word`s,** and so are the candidates,
  whether they come from the answer list or, in fallback mode, the guess list.
  `bucketize` reads them directly.
- **Candidate membership** (tie-break rule 1, T6) is a binary search in the
  sorted candidates.
- **A letter mask** (a 26-bit number with one bit per letter in the word)
  arrives in T6 with tie-break rule 2, its first user.

*Why `Word`s and not strings:* comparing 5 small numbers is far cheaper than
comparing strings, and the scoring loop runs millions of times per hint.

*Why sorted:* tie-break rule 3 ("alphabetical") becomes comparing two `Word`s,
and membership becomes a binary search.

*Why the letter mask:* tie-break rule 2 ("most distinct untested letters")
becomes `std::popcount(guessMask & ~testedMask)`.

*Why there's no is-answer flag any more:* tie-break rule 1 means "still a
**candidate**", not "in the answer list". In fallback mode the candidates come
from the guess list, so an is-answer flag would give the wrong answer.
Nothing else needed it.

### 5.2 How a feedback pattern is stored

- The public interface keeps the spec's `FeedbackPattern`
  (`std::array<Tile, 5>`), because it's easy to read and to write tests with.
- Internally, a pattern is packed into one number 0–242: `Gray = 0`,
  `Yellow = 1`, `Green = 2`, and position `i` is worth `3^i`. All gray is 0,
  all green is 242.
- `encode` turns a pattern into its code, with a test that all 243 patterns
  get different codes from 0 to 242. (No `decode`: nothing needs to turn a
  code back into a pattern. Add it the day something does.)

*Why 0–242:* there are 3 colors and 5 positions, so exactly 3⁵ = 243 possible
patterns. That fits in one byte, and grouping candidates needs no hash map:
it's an array of 243 counters, one per pattern.

### 5.3 The input boundary

All checking and converting of outside input happens in one place in the core:

| Function | Input | Output |
|---|---|---|
| `Word::parse(text)` | any text | a valid word (5 letters, converted to lowercase), or nothing |
| `parsePattern(text)` | text like `"XYGXX"` (also accepts lowercase) | a `FeedbackPattern`, or nothing |
| `statusToText(status)` | a `Status` | `"Ok"`, `"InvalidWord"`, … |
| `makeStrategy(name)` | `"minimax"` or `"entropy"` | a strategy, or nothing |

- `addFeedbackPattern` calls `Word::parse`. Nothing → `InvalidWord`.
- `setStrategy(name)` calls `makeStrategy`. Nothing → `UnknownStrategy`.
  `setStrategy(pointer)` with an empty pointer → `UnknownStrategy`.
- Past this boundary, the code only ever sees a `Word` (always 5 lowercase
  letters), so no internal function has to check again.
- **`InvalidPattern` in C++:** a `std::array<Tile, 5>` always has 5 values, so
  the only invalid native pattern is a `Tile` value outside the three colors.
  `addFeedbackPattern` checks each value is one of the three. In practice
  `InvalidPattern` comes from `parsePattern` failing on text from JavaScript,
  which the wrapper reports as `InvalidPattern`.

*Why one boundary:* draft 1 split this between the C++ core and the
JavaScript wrapper. The wrapper isn't measured by coverage, so the bad-input
handling would have been the one untested part. Putting it all in the core
means it's tested natively at 100%, and the wrapper only registers functions.

*Why a `Word` type instead of `std::string` inside:* a `Word` can only be
created through `Word::parse`, so an invalid word can't exist past the
boundary. That rules out the out-of-bounds read the reviewers found in draft
1 (`counts['S' - 'a']` with uppercase input) by construction, instead of
relying on every function remembering to check.

### 5.4 The feedback function

The algorithm, on two `Word`s:

```
counts[26] = for each answer position that is NOT green, add 1 for its letter
pass 1: each position where guess letter == answer letter → Green
pass 2: each non-Green position, left to right:
        counts[guess letter] > 0 → Yellow, counts[guess letter] -= 1
        otherwise → Gray
```

This is exactly the shared spec's rule. The `counts` array is what handles
repeated letters: once the answer's copies of a letter are used up by Greens
and earlier Yellows, any further copy in the guess is Gray.

- **Internally**, the fast version works on letter numbers and returns the
  0–242 code. The scoring loop calls it.
- **The public `computeFeedback(guess, answer)`** from the spec takes text,
  converts both words through `Word::parse` (so either case works), and calls
  the fast version. Passing something that isn't a 5-letter word is a
  programming error, caught by an assertion.

*Why two layers but one implementation:* the spec says there's only one
feedback implementation in the project. The public function is a thin
wrapper over the fast one, not a second copy of the rule. (The test-only
simple version in §8.1 is separate on purpose; see §11.)

### 5.5 Game state: what's stored, what's recomputed

**Stored** (the only things that change):
- the veteran words (as the spec's `VeteranWord`: word and pattern),
- the hard-mode flag,
- the strategy,
- the word lists (fixed for the solver's lifetime).

**Recomputed after every change**, by one function
`GameView derive(lists, veteranWords, hardMode)`:
- **solved**: the last veteran word's pattern is all green;
- **candidates**: answer-list words consistent with every veteran word; if
  there are none, guess-list words consistent with every veteran word;
- **fallback**: true when the candidates came from the guess list;
- **tested letters**: all veteran words' masks combined;
- **guess pool**: the whole guess list, or in hard mode only the allowed
  guesses.

How each function then works:

| Function | Behavior |
|---|---|
| `addFeedbackPattern(word, pattern)` | Check input (§5.3). If the pattern is all green: add the word, the game is solved, return `Ok` (§5.14). Otherwise: build the new veteran list, `derive` it, and if there are no candidates return `ImpossibleState` and throw the new list away. Otherwise keep the new list and its view. |
| `getBestGuess()` | Solved: the winning word. No veteran words: the first guess (§5.11). Otherwise: rank the guess pool (§5.6–5.8). |
| `setStrategy(...)` | Validate (§5.3), swap the strategy. Game state doesn't change. |
| `setHardMode(on)` | Set the flag. Nothing else changes: the hard-mode pool is filtered when a hint is asked for (§5.12). |
| `reset()` | Clear the veteran words, `derive` again. Keeps the strategy and the hard-mode setting (see §11). |

*Why recompute instead of updating piece by piece:* in draft 1, each function
had to update five fields in the right order, and a mistake would leave them
disagreeing. Now there's one source of truth (the veteran words) and one
function that turns it into everything else. "State unchanged on error"
happens automatically, because nothing is kept until `derive` succeeds.

*Why fallback doesn't need a stored flag:* adding a clue can only remove
words, never bring one back. So once no answer-list word fits, no answer-list
word will fit again for the rest of that game. "Fallback lasts until
`reset()`" (the spec's rule) is then simply what `derive` computes every time.

*Why this isn't slow:* `derive` checks each list word against each veteran
word: at most 14,855 × 6 feedback calls, plus the same number of hard-mode
checks. That's tiny compared with ranking guesses (§5.9).

*What happens with unusual input, for free:*
- **A word the game accepted but that isn't in the guess list** is just a
  `VeteranWord`; `derive` only needs its letters. No special case.
- **The same word entered twice** with the same pattern changes nothing. With
  a different pattern, no word can fit both, so it returns `ImpossibleState`.
  A test pins both.

### 5.6 Scoring: buckets, strategies and ranking

The strategy interface (a change from the spec; see §11):

```cpp
using BucketCounts = std::array<std::uint32_t, 243>;

class ScoringStrategy {
public:
    virtual ~ScoringStrategy();
    // Higher is always better.
    virtual double score(const BucketCounts& counts, std::size_t total) const = 0;
    virtual std::string_view name() const = 0;   // "minimax" or "entropy"
};
```

- **Minimax:** minus the largest count.
- **Entropy:** `Σ (n/N) · log₂(N/n)` over the non-zero counts, with `N = total`.

Internal functions (in `ranking.cpp`), which the solver, the tests and the
tools all share:

| Function | What it does |
|---|---|
| `bucketize(guess, candidates)` | Fills the 243 counters for one guess. Lets a test check "the worst bucket for this guess is 3" directly, as the strategy specs ask. |
| `scoreGuesses(pool, candidates, strategy)` | Runs `bucketize` and `score` for every guess in the pool. |
| `pickWinner(scored, isCandidate, testedMask)` | The tie-break (§5.7). Pure: takes scores, returns one guess. |
| `rankBest(pool, candidates, strategy, testedMask)` | The shortcuts (§5.8), then `scoreGuesses`, then `pickWinner`. |

*Why the solver builds the buckets and the strategies only score them:*
filling the counters is where nearly all the time goes, and it's identical
for both strategies. Doing it once means it's written and optimized once,
and the strategies are tiny functions that tests can feed made-up counts:
the entropy spec's "4 buckets of 1 scores 2 bits" is a one-line test.

*Why `rankBest` is separate from `getBestGuess`:* `getBestGuess` returns the
stored first guess on an empty game. The precompute tool needs the opposite:
run the full ranking on an empty game. With `rankBest` as its own function,
both use the same code, and tests can call it on tiny lists.

*Why the destructor is declared, not `= default` in the header:* defining it
once in a `.cpp` file gives the coverage tool one destructor to see, instead
of several compiler-generated copies that may show up as "never run" (§8.6).

### 5.7 The tie-break

`pickWinner` does:

1. Find the best score.
2. Keep every guess whose score is within `1e-9` of it (the spec's definition
   of tied).
3. Rule 1: if any kept guess is a candidate, keep only the candidates.
4. Rule 2: keep the guesses with the most distinct untested letters.
5. Rule 3: take the lowest index (alphabetical).

*Why not sort with a comparison function:* "within `1e-9`" isn't transitive:
A can tie with B and B with C while A doesn't tie with C. Sorting with such a
comparison is undefined behavior in C++, which in practice means different
results on different compilers. Comparing everything to one anchor (the best
score) avoids that. This is the definition you approved for the spec.

*Why `1e-9` for minimax too:* minimax scores are whole numbers, at least 1
apart, so `1e-9` means "exactly equal" for minimax. One constant, no
per-strategy setting.

*Why the tolerance also protects WebAssembly:* `log2` in the WebAssembly
build comes from a different math library than the native build, so entropy
scores can differ in the last few digits, roughly `1e-15` to `1e-14`. That is
far below `1e-9`. A score sitting almost exactly on the `1e-9` boundary could
in theory still flip, which is why native and WebAssembly results are
compared on every position, not assumed equal (§8.4). The solver is also
compiled with `-ffp-contract=off`, so the compiler can't merge
multiply-and-add steps differently on different machines.

### 5.8 Exact shortcuts

*Not built: T6 tried this and deleted it after review; see "Done in T6". Kept
here as the record of the idea.*

These skip work without ever changing the result, for both strategies.
`rankBest` applies them before the full scoring loop:

1. **One candidate left:** return it.
2. **Two candidates left:** guessing either one splits them perfectly (each
   lands in its own bucket), which is the best possible score for both
   strategies. A non-candidate can at best tie, and tie-break rule 1 then
   prefers a candidate. So `pickWinner` chooses between the two directly.
3. **Score the candidates first.** If some candidate splits every candidate
   into its own bucket (all buckets of size 1), that is the best possible
   score for both strategies (minimax worst bucket of 1, entropy of
   log₂ N). No non-candidate can beat it, and rule 1 would reject a tie. So
   the non-candidates are skipped.

*Why these are safe:* each one only skips guesses that provably can't win
under the spec's scoring and tie-break. They need no strategy-specific code,
so the design in §5.6 stays intact.

*How much they help:* shortcut 3 helps most when few candidates are left,
which is most turns after turn 2. It does not help turn 2 after a weak first
guess, which is the slow case (§5.9).

### 5.9 Speed: what's slow, and how it gets decided

The cost of one hint is about (guess-pool size) × (candidates left) feedback
calls.

- **Turn 1** is free: the first guess is stored.
- **After a good first guess**, a few hundred candidates remain at most.
- **The slow case is common, not rare:** players often ignore the hint and
  open with a weak word. That can leave well over 1,000 candidates, which is
  around 20 million feedback calls for turn 2. Whether that fits in 500 ms
  natively, and especially in WebAssembly, is unknown until measured.
- **Fallback early in a game** can be worse, because candidates then come
  from the 14,855-word list.

So the measurements come first (task T4), with defined worst cases instead
of "a few bad guesses":
- **Worst first guess:** for every one of the 14,855 possible first guesses,
  find its largest bucket over the answer list; the largest of those is the
  worst possible turn 2.
- **Worst fallback position:** the first guess and pattern that leave no
  answer-list word but the most guess-list words.

**T4 results** (`solver/bench/nudge_spike`, best of 5 runs, the developer's
laptop; one hint = bucket counts for all 14,855 guesses against the candidates):

| Worst case | Candidates left | Native (MinGW GCC 12, -O3) | WebAssembly (Emscripten 6.0.12, Node 24.19), warm | WebAssembly, cold |
|---|---|---|---|---|
| Opener: `xviii`, all gray | 1,549 answers | 246–254 ms | 321–327 ms | 324–353 ms |
| Fallback: `sughs`, only the last S green | 2,398 guess-list words | 313–323 ms | 428–434 ms | 430–433 ms |

"Warm" is the best of 5 runs after the worst-case search has already run the
same code millions of times. "Cold" is a fresh process that only filters the
candidates once and computes one hint, the way a player's first hint runs
(`nudge_spike <opener> <pattern> answers|guesses`); ranges are over 3 rounds.
Cold and warm are close: the worst cold run was 353 ms for the opener case,
once, and 433 ms for fallback.

Native and WebAssembly produced identical checksums, so both did exactly the
same work with the same results. WebAssembly took about 1.3 to 1.4 times as
long as native.

**Decision: the budget is met; no design change.** Both worst cases fit in
500 ms in WebAssembly, warm and cold. The fallback case has the least room
(433 ms at worst, about 13% to spare). These were measured in Node (V8);
Firefox runs WebAssembly with a different engine, so the extension's
end-to-end check measures the real browser. The timings also don't yet include the strategies' own
scoring (entropy adds one `log2` per non-empty bucket). T11 measures the real
strategies. If entropy's fallback case then goes over, the first fix is
internal and needs no spec change: compute the 0–242 code directly in
`feedbackCode` instead of building five tiles and encoding them.

**T6 review timings** (native, -O3, full `rankBest` including scoring; from
the reviewers' scratch measurements, so T11 starts from them):

| Case | Minimax-shaped scoring | Entropy-shaped scoring |
|---|---|---|
| Worst opener (1,549 candidates) | 250–254 ms | 286–290 ms |
| Worst fallback (2,398 candidates) | 316–325 ms | 337–341 ms |
| Empty game (2,309 candidates, until T9 stores first guesses) | 372–376 ms | not measured |

At T4's WebAssembly ratio of 1.3–1.4, entropy's fallback case is about
445–477 ms and the empty game about 485–520 ms, so T9's stored first guesses
are needed to keep turn 1 inside the budget.

**If T4 had shown the budget was missed**, these were the options, each with
what it actually removes (kept for T11, in case the real strategies change the
picture):

| Option | Removes | Cost |
|---|---|---|
| Feedback table for the answer list, built on first use | The feedback calculation itself for normal turns (it becomes a memory lookup) | About 34 MB of memory, which needs Emscripten's `ALLOW_MEMORY_GROWTH` (its default starting memory is 16 MB). Building it takes noticeable time, so it must be built in steps between hints, not all at once. **Doesn't help fallback.** |
| Minimax: stop counting a guess once one bucket is larger than the best so far | Most of the counting for minimax only | Needs a new strategy hook, which is a spec interface change. Doesn't help entropy. |
| Accept a slower hint in fallback mode | Nothing; changes the requirement | **A spec decision:** the spec currently says 1 second on every turn. |

*Why not build these up front:* each adds memory, complexity or a spec
change. Measuring first means only the ones that are actually needed get
built.

### 5.10 Word lists inside the binary

- `scripts/import_word_lists.py` writes `data/answers.inc` and
  `data/guesses.inc` directly. Each file starts with comment lines recording
  the source, the exact commit and the date, then has one word per line in
  C++ syntax:

  ```
  // source: https://github.com/tabatkins/wordle-list (file "words")
  // commit: <sha>   retrieved: <date>
  "aahed",
  "aalii",
  ...
  ```

- `word_lists.cpp` includes them into constant arrays:
  `constexpr std::string_view kGuesses[] = {
  #include "guesses.inc"
  };`

*Why include files instead of a generator:* draft 1 had a second Python
script that ran on every build to turn text into C++. With the import script
writing C++-ready lines, there's one file per list, nothing to drift out of
sync, and no Python needed in the C++ build (or in the WebAssembly build).
A word-list update still shows up in a code review as added and removed
lines.

*Why compile the lists in at all:* in the browser, WebAssembly can't open
files on its own; the extension would have to fetch them and pass them in.
Compiling them in removes that step and guarantees native tests, the
benchmark and the WebAssembly build use identical data. The words take about
75 KB.

### 5.11 Word lists, injection and first guesses

`WordLists` holds the answer list, the guess list, and **the first guess for
each strategy**:

- `WordLists::builtin()` is the compiled-in data (§5.10), with first guesses
  from `data/first_guesses.inc`.
- `WordLists::fromWords(answers, guesses)` builds lists for tests. It sorts
  them, checks every word is 5 letters a–z, removes nothing silently (a
  duplicate is an error), and checks every answer is in the guess list. It
  stores no first guesses unless a test passes some (a third argument; see
  "Done in T9").
- The solver's constructor is
  `WordleSolver(std::unique_ptr<ScoringStrategy>, bool hardMode, const WordLists& = WordLists::builtin())`.
- On an empty game, `getBestGuess()` returns the stored first guess for the
  current strategy. **If none is stored** (test lists), it computes it with
  `rankBest`.

*Why first guesses belong to the word lists:* the right first guess depends
on which words are in play. In draft 1 it came from the strategy, so a test
using a tiny 6-word list would have started with "crane" or similar, a word
not even in that list. Storing it with the lists, and computing it when it
isn't stored, makes tiny-list tests behave like real games.

*Why a default argument instead of a second constructor:* it keeps the spec's
constructor exactly as written for normal use, and adds the test option
without a second constructor to maintain.

*Why `fromWords` checks its input:* a badly built test list (unsorted, or
with an answer missing from the guess list) would silently break the index
assumptions in §5.1 and make tests pass or fail for the wrong reason.

**The precompute tool (`nudge_precompute`)** runs `rankBest` on an empty game
for each strategy using the built-in lists and writes
`data/first_guesses.inc`, which is committed.

*Why a tool you run instead of a build step:* the WebAssembly build can't run
a native program as part of its own build, and the stored first guess is what
keeps turn 1 instant in the browser. So the value is computed natively once,
committed, and compiled into both builds. A slow test recomputes it and fails
if the file is out of date.

*Bootstrapping:* the file starts with no stored guesses. Until the tool has
been run, `getBestGuess()` simply computes the first guess (the same path as
test lists). The slow test fails until the file is filled in, so it can't be
forgotten.

### 5.12 Hard mode

- **The rule** ("rule B", decided in the shared spec) is one function,
  `isHardModeLegal(guess, constraints)`:
  - every Green letter stays in its position;
  - every letter appears at least as many times as it was lit up (Green or
    Yellow) in any one earlier guess;
  - Yellow letters may be anywhere; Gray tiles add no requirement.
- **The constraints come from the veteran words** (`hardModeConstraints`): a
  list of fixed positions, and a 26-entry table of minimum counts (for each
  letter, the highest Green-plus-Yellow count from any one veteran word).
  Checking a guess is then five position checks and a count comparison.
- **The pool is filtered when a hard-mode hint is asked for**, by running the
  rule on every guess-list word. Nothing about hard mode is stored in the game
  state except the flag, so `derive` doesn't depend on the mode, and
  `setHardMode` can't leave a stale pool behind. One pass over 14,855 words is
  small next to ranking them. (T10 first stored the pool in the view; the T10
  review replaced that.)
- **The pool is never empty,** which `getBestGuess` asserts: there is always a
  candidate (`addFeedbackPattern` keeps no state without one), every candidate
  is a guess (`fromWords` checks answers are guesses; fallback takes them from
  the guess list), and a candidate fits all the feedback, so it has every
  Green letter in place and at least every copy that was lit up.

*Why rule B (counted letters) instead of "each letter at least once":* it's at
least as strict as whatever NYT does. If NYT only checks one copy, the solver
skips a few guesses NYT would allow, which costs almost nothing. If the solver
were looser than NYT, it could suggest a word the game rejects, which would
make the extension look broken. Being strict is the safe direction, so the
original game's code no longer has to be checked before building this.

*Why the rule lives in one function:* if it ever needs to change, only this
function and its test checker change.

*Why it's checked by a separate test rule:* see §8.1.

### 5.13 Fallback mode

Fallback is part of `derive` (§5.5): if no answer-list word is consistent,
candidates come from the guess list. There's no separate fallback code path
in scoring; the same loop runs on whichever candidates `derive` produced.

**A known weakness, documented, not fixed** (risk R7): if the real answer is
missing from the answer list but other answer-list words still fit the clues,
the solver keeps narrowing the wrong set. It can even present a wrong word as
the last candidate. Fallback only starts once that word is ruled out. A test
pins this behavior so it can't change unnoticed, and §11 asks whether you
want a spec decision on it.

### 5.14 All green means solved

Per your decision, `addFeedbackPattern` checks for an all-green pattern
**before** any filtering. If it's all green, the word is added, `derive`
marks the game solved, and the call returns `Ok`, even for a word in neither
list. `getBestGuess()` then returns that word.

*Why before filtering:* a winning word that's in neither list has no
candidate that fits it, so filtering first would report an impossible board
on a game the player just won. Checking first is what makes your rule hold.

**After the game is solved**, `addFeedbackPattern` returns `GameOver` and
changes nothing, since the real game accepts no more guesses. The solver
never resets itself: the extension calls `reset()` when the Wordle board on
the page is empty, which is when a new game starts.

### 5.15 The WebAssembly interface

`solver/wasm/bindings.cpp` uses Emscripten's **embind** to register a
JavaScript-friendly `Solver` class. It only connects names to the core
functions from §5.3; it contains no checks or conversions of its own:

- `new Solver(hardMode)` creates a solver with the default strategy
  (minimax), so the constructor can't fail.
- `solver.setStrategy("entropy")` → `"Ok"` or `"UnknownStrategy"`.
- `solver.addFeedbackPattern("crane", "XYGXX")` → a status text.
- `solver.getBestGuess()`, `getRemainingCount()`, `isUsingFallback()`,
  `setHardMode(on)`, `reset()`.
- Build flags: `-lembind`, `-sMODULARIZE=1`, `-sEXPORT_ES6=1`, plus
  `-ffp-contract=off` like the native build. (Emscripten's Release build
  already sets the optimization level.)

*Why a wrapper at all:* the C++ interface passes strategies as `unique_ptr`
and patterns as arrays of enums, which don't map cleanly to JavaScript.

*Why the constructor uses a default strategy:* a constructor can't return a
status. Starting with minimax and letting JavaScript call
`setStrategy(name)` means every JavaScript-reachable failure returns a status
instead of crashing.

*Why `MODULARIZE` and `EXPORT_ES6`:* the output becomes a normal JavaScript
module the extension can import, instead of a script that sets global
variables.

---

## 6. Data preparation

### 6.1 `scripts/import_word_lists.py`

Run by hand only when you decide to update the lists. It:

1. downloads `wordles.json` from stuartpb/wordles and `words` from
   tabatkins/wordle-list **at specific commits**, not the latest version;
2. converts to lowercase and sorts;
3. checks: every word is exactly 5 letters a–z, no duplicates, every answer
   is in the guess list. Any failure stops the script with a clear message;
4. writes `data/answers.inc` and `data/guesses.inc` with LF line endings and
   the source comment lines (§5.10).

The script is split into small functions (download, parse, check, write).
Everything except the download is tested with pytest at 100% coverage.

*Why pin commits:* "latest" can change without warning. A pinned commit means
running the script twice gives identical files, and an update is a deliberate
change you can review.

*Why the download isn't tested:* it needs the network. It's kept to one small
function marked as excluded from coverage, with a comment saying why.

*Why the script both lowercases and has tests for uppercase input:* the
tests check that uppercase input comes out lowercase, not that it's
rejected.

### 6.2 `data/NOTICE`

Records both sources, the commits, and their licenses: the MIT license text
for tabatkins/wordle-list, and a note that stuartpb/wordles has no license
file and describes common word lists as commonly considered public domain.

*Why:* MIT requires its notice to be included with copies, and the
extension will distribute these words. Writing the sources down also makes
a later update easy. (This is record-keeping, not legal advice.)

---

## 7. Tasks, in build order

Each task is small enough to finish and review on its own, ends with passing
tests, and is one commit. **Every task keeps coverage at 100%**: a task isn't
done if it adds a line no test runs.

### T1. Project skeleton and CI

- Top-level and `solver/` CMake files; GoogleTest through `FetchContent`;
  the `NUDGE_BUILD_TESTS` and `NUDGE_COVERAGE` options.
- `.gitattributes`, `.clang-format`, `.clang-tidy`, `.gitignore`
  (`build*/`, `node_modules/`).
- GoogleTest's download is pinned by its SHA256 hash, so a changed archive
  stops the build instead of being used silently.
- One small real library function with a test, so the coverage gate measures
  something.
- CI jobs: GCC build and fast tests, Clang build and fast tests, lint,
  coverage (section 9).
- **Prove the coverage gate works:** on a throwaway branch, add a function
  with no test and confirm CI fails. Also confirm the pinned gcovr supports
  `--fail-under-function`.
- **Check sanitizers locally:** try `-fsanitize=address` and
  `-fsanitize=undefined` with your MinGW GCC, and record what works. (AddressSanitizer
  is expected not to; CI covers it either way.)
- Commands from §4.2 merged into `CLAUDE.md`.

**Done when:** CI is green, and the throwaway branch showed the gate going
red.

*Why first:* everything else depends on building and testing. Setting up CI
while there's almost no code is easy; adding it to a large project means
fixing many problems at once. Proving the gate fails matters because a gate
that measures nothing passes silently.

### T2. Words, patterns and the feedback function

- `Word` and `Word::parse`, `parsePattern`, `statusToText`, the 0–242
  `encode`, the fast feedback function and the public `computeFeedback`.
  (`patternToText` and `decode` were dropped in T2: no function returns a
  pattern to JavaScript, and nothing turns a code back into a pattern.)
- `tests/reference/simple_feedback`: a deliberately simple version written
  with strings.
- Tests:
  - the spec's four hand-checked cases, in upper and lower case;
  - `"CRANE"`, `"Crane"`, `"crane"` all become `crane`; 4- and 6-letter
    words and non-letters are rejected;
  - pattern text: valid, wrong length, wrong characters;
  - all 243 patterns get different codes from 0 to 242;
  - **exhaustive comparison** with the simple version on every pair of
    5-letter strings over a 3-letter alphabet (243 × 243 = 59,049 pairs),
    where nearly every pair has repeated letters, including triples and
    more;
  - comparison on 100,000 random pairs of 5-letter strings over a–z, with a
    fixed seed (the real word lists only arrive in T3);
- **Mutation check** (§8.7): remove the step that uses up a letter's copy
  and confirm the tests fail, then restore it. Done in T2: 4 tests failed
  (both hand-checked case tests and both reference comparisons).
- The `sanitizers` CI job, and the CI actions moved to their Node 24
  versions (v7).

**Done when:** all fast tests pass.

*Why second:* everything else depends on feedback being right. The
three-letter alphabet makes repeated letters common, which random real words
rarely produce.

### T3. Word lists

- `import_word_lists.py`, run once to create `data/answers.inc` and
  `data/guesses.inc`; `data/NOTICE`; `WordLists::builtin()` and
  `WordLists::fromWords()`; `scripts/.coveragerc` (moved here from T1, since
  nothing reads it before the script exists).
- C++ tests: 2,309 answers, 14,855 guesses, every answer in the guess list,
  sorted, no duplicates, every word 5 letters a–z. `fromWords` rejects
  unsorted input it can't fix, duplicates, and an answer missing from the
  guess list.
- pytest tests at 100%: valid lists, uppercase input lowercased, and each check
  failing on bad input (a 6-letter word, a duplicate, an answer missing from
  the guess list). A CRLF input file comes out LF.
- Done in T3, beyond the list above: `word.hpp` moved to the public headers,
  since `WordLists` hands out `Word`s; `fromWords` also rejects an empty
  answer list; the import script creates `data/` if it's missing (found by
  the first real run). (T3 also stored answers as 16-bit positions with a
  65,536-guess limit; T5's review removed both, see §5.1.)

### T4. Speed and WebAssembly spike

- Install emsdk (pinned version) and Node 24.
- Build a minimal embind module containing only the feedback function and a
  timing loop that, for given candidates, fills the 243 counters for every
  guess in the pool.
- Compute the two defined worst cases from §5.9 natively (worst first guess,
  worst fallback position).
- Time them natively and in Node.
- Add the CI job that builds the WebAssembly module.

**Done when:** the numbers are written into this plan (§5.9) and one of these
is decided: "the budget is met, nothing more needed", or "use option X from
§5.9", or "this needs a spec decision".

*Why fourth:* this is the biggest unknown in the plan. If WebAssembly is too
slow, the fix could change how the scoring loop is built. Finding out before
T5–T8 are built on it is much cheaper than finding out at the end. It also
gets the Emscripten install, the riskiest setup step, done early.

### T5. Solver state

- `GameView` and `derive`; `WordleSolver` with `addFeedbackPattern`,
  `getRemainingCount`, `getVeteranWords`, `isUsingFallback`, `reset`, and
  `setStrategy(pointer)`. A trivial test-only strategy stands in until T7.
- Fallback, impossible state and all-green, all handled in `derive` and
  `addFeedbackPattern` from the start (§5.5, §5.13, §5.14).
- Fast tests, on tiny lists:
  - a 5-letter word not in the guess list is accepted and narrows the
    candidates;
  - bad words → `InvalidWord`; an out-of-range tile → `InvalidPattern`;
  - candidate counts after known guesses match hand-checked counts;
  - an answer missing from the answer list switches to fallback, and fallback
    stays on until `reset()`;
  - a pattern no word matches → `ImpossibleState`, with the remaining
    count, veteran words and fallback flag unchanged;
  - all-green for a word in neither list → `Ok`, solved;
  - any guess after the game is solved → `GameOver`, veteran words unchanged;
  - the same word twice: same pattern changes nothing, different pattern →
    `ImpossibleState`;
  - `reset()` clears veteran words and fallback, keeps strategy and hard mode;
  - `setStrategy(nullptr)` → `UnknownStrategy`, state unchanged;
  - the "fallback starts late" behavior from §5.13, pinned.

*Why fallback and impossible state are here, not later:* draft 1 added them in
T10, which meant rewriting `addFeedbackPattern` after it was written. With
`derive`, they're part of the same small function from the start.

**Done in T5, and how it differs from the list above:**
- `derive` computes only what T5 uses: solved, fallback and the candidates.
  Tested letters arrive in T6 and the hard-mode pool in T10, each with its
  first user. The hard-mode flag isn't stored yet either (Clang rejects a
  stored field nothing reads); the constructor keeps the spec's parameter.
- Candidates are `Word`s, not guess-list positions: `bucketize` reads them
  directly, and a winning word in neither list has no position. Following
  T5's review, `WordLists::answers()` also returns `Word`s now; the 16-bit
  `Index`, its 65,536-guess limit and its test are gone (§5.1).
- `WordleSolver` copies its `WordLists` (passed by value), so a temporary list
  can't leave it with a dangling reference.
- "The same word twice with the same pattern changes nothing" means the
  candidates don't change. The veteran words show both rows, like the board.
- The coverage gate now uses gcovr's `--exclude-noncode-lines`. GCC puts the
  exception clean-up of a function returning a `std::string` or `std::vector`
  on its closing `}`, which no test can run. The option drops exactly those
  lines (190 counted lines became 188), and the gate was rechecked to still
  fail on a really untested function.

### T6. Ranking and tie-break

- `bucketize`, `scoreGuesses`, `pickWinner`, the shortcuts, `rankBest`, and
  `getBestGuess` (computing the first guess on an empty game for now).
- Fast tests:
  - `pickWinner` given scores directly: one test per rule; one only rule 3
    resolves; none of the tied guesses is a candidate; two scores `5e-10`
    apart count as tied, two scores `2e-9` apart don't;
  - the A/B/C chain from the review: with best `s`, `s − 0.8e-9` is tied
    and `s − 1.6e-9` is not;
  - each shortcut returns the same result as the full loop on small cases;
  - one candidate left → returns it; solved → returns the winning word.
- **Mutation check:** flip rule 2's comparison and confirm a test fails.

*Why the tie-break gets scores handed to it:* the strategy only sees bucket
counts, not which guess they belong to, so a fake strategy can't easily
create a tie on a chosen guess. Testing `pickWinner` on its own avoids that
problem entirely.

**Done in T6, and how it differs from the list above:**
- **No shortcuts.** T6 first built shortcut 3 alone (1 and 2 are special
  cases of it). The multi-model review then found three problems: it was only
  exact for strategies that score an all-separate split *strictly* better (the
  written rule allowed ties, and a constant-score strategy gave different
  answers); it ignored the guess pool, so in hard mode (T10) it could suggest
  an illegal word; and it saves almost nothing, since it can only run with at
  most 243 candidates, where full scoring takes at most about 65 ms, while the
  slow hints have over 1,500 candidates. So it was deleted. A single candidate
  still wins through rule 1: it scores the best possible, and any guess that
  ties it is not a candidate.
- **No separate `scoreGuesses`:** it's the loop in `rankBest`, its only user.
- **`assert`s** guard `pickWinner`'s preconditions (some guesses, sorted
  candidates) and catch a NaN score, which would otherwise drop out of every
  tie comparison and crash.
- **Tried letters only matter between tied non-candidates.** Every tried
  letter's presence is fixed by its color, so all candidates contain the same
  tried letters and rule 2 ranks them the same with or without them. The
  solver test for tried letters is built on that case.
- **Mutation checks** (five, not one): rule 2 reversed, rule 1 removed, the
  tolerance removed, rule 2 applied before rule 1, and tried letters never
  recorded. All five are caught. The rule-order one is caught only by
  `Rule1ComesBeforeRule2`, the gap the review found. (While the shortcut
  existed, a mutation making it too eager first survived, which is how its
  `bills`/`fills`/`hills` test case came about.)
- **Determinism** is tested on 10 fresh solvers, as the spec asks, not by
  asking one solver twice.

### T7. Minimax strategy

- `MinimaxStrategy`.
- Fast tests: hand-checked worst-bucket sizes for several guesses on a small
  list, using `bucketize`; a case where the best guess is **not** a candidate.
- The tree-walk test helper (§8.2) with the independent feedback checker.
- Slow tests: every one of the 2,309 answers in normal mode solved in at most
  6 guesses; determinism on fresh solvers (§8.3).

**Done when:** every answer is solved within 6 guesses. If any isn't, stop:
that's a spec must-have failing, and the fix is your decision (risk R1).

**Done in T7: R1 is answered.** Minimax solves all 2,309 answers, the worst
case in 5 guesses:

| Guesses | 2 | 3 | 4 | 5 | Average |
|---|---|---|---|---|---|
| Answers | 62 | 975 | 1,209 | 63 | 3.551 |

- It opens with `olate`. The tree has 2,480 positions; at every one, the
  solver's candidate *count* matched the independent reference feedback and
  the solver was not in fallback mode (a count check, not word by word: the
  solver exposes no candidate list), and a second walk with all-new solvers
  played the identical tree. Each walk takes about 5 to 7 seconds natively;
  the slow suite runs in CI's `gcc` job.
- **The exact tree is pinned** (`PlaysTheRecordedTree`): the opener, the
  distribution above and the 2,480 positions. The T7 review showed that
  without it, rule 2 or 3 reversed, tried letters ignored, or even a different
  strategy all passed the slow suite, since they still solve everything within
  6. With it, all three of those mutations fail it. A deliberate change to
  the strategy, tie-break or word lists has to update these values.
- **The cross-check's detectors are tested** (fast tests that make each one
  fire), so "0 mismatches, 0 replay failures" can't silently become a no-op.
  Deleting either one fails a test.
- CI test steps use `--no-tests=error`, so a step that finds no tests fails
  instead of passing.
- **Mutation checks on the slow test:** a constant score (every guess ties)
  made 34 answers need 7 to 10 guesses, and removing the feedback step that
  uses up a letter's copy gave 518 positions where solver and reference
  disagreed, plus 174 unsolved answers. Both fail the test. The second one is
  why the walk uses the reference feedback: with the solver's own, solver and
  simulation would share the bug and agree.
- The tree walk itself has fast tests on a two-answer list (worked out by
  hand) and on a strategy that never makes progress (the depth limit stops it).
- `MinimaxStrategy` replaced the two `WorstBucketStrategy` test copies.

### T8. Entropy strategy and strategy names

- `EntropyStrategy`; `makeStrategy(name)` and `setStrategy(name)`.
- Fast tests: the spec's 2-bit and 0-bit cases; a non-candidate best guess;
  `setStrategy("knuth")` and `setStrategy("Minimax ")` → `UnknownStrategy`,
  `"minimax"` and `"entropy"` → `Ok`; **switching strategy mid-game keeps the
  veteran words and candidates**.
- Slow tests: the same as T7.

**Done in T8.** Entropy also solves all 2,309 answers within 6 guesses,
with a better average than minimax but a worse worst case:

| Strategy | Opener | 2 | 3 | 4 | 5 | 6 | Average | Positions |
|---|---|---|---|---|---|---|---|---|
| Minimax | `olate` | 62 | 975 | 1,209 | 63 | 0 | 3.551 | 2,480 |
| Entropy | `tarse` | 61 | 1,257 | 936 | 53 | 2 | 3.428 | 2,470 |

- The two answers that take 6 are *hover* and *poker* (the -over and -oker
  families). That is expected: minimax minimizes the worst group, while
  entropy maximizes the average information.
- The slow suite is one parameterized suite (`Strategies/FullGames.*`), run
  for both names through `makeStrategy`. Each entropy walk takes about 9
  seconds natively.
- **Each tree is now pinned move by move, not just by its shape.** Besides
  the opener, the distribution and the position count, `PlaysTheRecordedTree`
  checks `movesDigest`: a 64-bit FNV-1a checksum of every `position=guess`
  line, the same on every platform (`std::hash` is not). The T8 review
  pointed out that a different guess deep in the tree can leave the shape
  unchanged. A fast test checks the digest against an independent Python
  FNV-1a, and that changing one move with the same shape changes it.
  Digests: minimax `0xf1e24eb8b6f55487`, entropy `0x17eec0cb3e927250`.
- **The slow suite also runs in CI's `clang` job,** so two compilers must
  play the identical trees. Native against WebAssembly comes in T12 (§8.4).
- **The 1e-9 tolerance matters for real games, not just in theory.** With it
  set to 0, one entropy move in the real tree changes (one answer goes from 3
  to 4 guesses, 2,471 positions), and the pinned tree catches it. Widening it
  to 1e-6, 1e-4 or even 1e-2 changes nothing: in the real games, near-ties are
  either rounding-sized (about 1e-16) or far apart. The fast test
  `RoundingDoesNotBreakATie` shows the cause on two hand-built probes: equal
  groups (3, 1, 1, 1) in a different pattern order add up to scores that
  differ in the last digit, so without the tolerance the wrong probe wins.
  The test asserts that last-digit gap itself (`EXPECT_LT`), so a platform
  that adds them up equal fails loudly instead of quietly no longer testing
  the tolerance.
- `makeStrategy` lives in `text_format` with the other name conversions
  (§5.3). `setStrategy(name)` is one line: `setStrategy(makeStrategy(name))`.
- With two `setStrategy` overloads, `setStrategy(nullptr)` no longer compiles
  in C++20 (`nullptr` converts to both a pointer and a `string_view`). That
  is a compile error, never a wrong result, so the T5 test now passes an
  empty `std::unique_ptr` instead.
- A tiny list where minimax and entropy pick different guesses checks that
  the name really swaps the strategy. The mid-game test plays a clue that
  removes two of the eight words first, so a switch that reset the
  candidates would show 8 instead of 6 (the first version used a clue that
  removed nothing, so it couldn't tell; found in review).
- The test helpers `w()` and `words()` live once in `tests/test_words.hpp`,
  instead of a copy in five test files. One copy of `words()` didn't sort
  its list, while `pickWinner` asserts a sorted candidate list in Debug builds.
- Mutation checks, each caught: the name ignored, natural log instead of
  log₂, no weighting by p, empty groups counted, `"entropy"` making minimax,
  and a name switch that resets the candidates.

### T9. Precomputed first guesses

- `nudge_precompute`; `data/first_guesses.inc`; `getBestGuess()` returns the
  stored guess when there is one.
- Fast tests: the stored first guess is returned on an empty built-in game;
  a tiny list computes its own; the first guess is the same in hard mode.
- Slow test: recomputing gives the same words as the file.

**Done in T9.** `data/first_guesses.inc` stores `olate` (minimax) and
`tarse` (entropy), the same openers the T7 and T8 walks measured. An empty
built-in game now answers at once instead of ranking the whole list (about
0.4 seconds natively per strategy, measured by the slow test that
recomputes it). The precompute tool computes both in under a second.

- **`fromWords` takes the first guesses as an optional third argument**, and
  `builtin()` passes the file's entries through it. So there is one check for
  all lists: a stored first guess must be a word in the guess list, or the
  lists are rejected (and `builtin()` stops with its existing error). It also
  lets a test store a first guess that ranking would never choose, which is
  the only way to show the stored value is really used: on the real lists,
  stored and computed are the same word.
- Stored by the strategy's `name()`, so a strategy without an entry simply
  computes its first guess, like the tiny test lists do.
- `nudge_precompute` (in `solver/tools/`, native only) prints the file to
  standard output: `nudge_precompute > data/first_guesses.inc`. On Windows
  the redirect writes CRLF line endings; `.gitattributes` (`eol=lf`) turns
  them back into LF on commit, and the compiler doesn't care.
- **The up-to-date check can't ask a solver**, because a solver now opens
  with the stored guess, which would compare the file with itself. The slow
  test `StoredFirstGuessIsUpToDate` calls `rankBest` directly, the way the
  tool does, for both strategies.
- The file started empty, as planned (bootstrapping in §5.11): the built-in
  test and the slow test failed until the tool had been run.
- The hard-mode test (`OnTheBuiltinListsInEitherMode`) passes trivially until
  T10 adds hard mode; it pins the spec's rule that the first guess is the same.
- Mutation checks, each caught: the stored guess ignored, the stored guess
  used on every turn, a stored guess not checked against the list, the
  lookup ignoring the strategy name, and a stale file (caught by a fast test
  and by the slow test, whose message says to rerun the tool).

### T10. Hard mode

- The hard-mode constraints in `derive`, `isHardModeLegal`, `setHardMode`.
- `tests/reference/simple_hard_mode`: rule B written separately from the
  spec's wording, with strings and letter counting (§8.1).
- Fast tests: hand-listed allowed and rejected guesses for several
  histories, including the spec's repeated-letter cases (after `SPEED` vs
  `ERASE`: `EASEL` allowed, `SENOR` rejected; after `SPEED` vs `ABIDE`: one E
  plus a D is enough); a candidate always passes; **turning hard mode off restores the
  full pool and keeps the veteran words**; hard mode together with fallback.
- Slow tests: every answer in hard mode, both strategies; every suggestion is
  allowed according to the **simple** checker; the number of games over 6
  guesses is recorded (not required to be zero, per the spec).

**Done in T10.** Hard mode solves every answer for both strategies, but not
always within 6, as the spec expected:

| Strategy, mode | Opener | 2 | 3 | 4 | 5 | 6 | 7 | Average | Positions |
|---|---|---|---|---|---|---|---|---|---|
| Minimax, hard | `olate` | 87 | 905 | 1,082 | 206 | 25 | 4 | 3.649 | 2,392 |
| Entropy, hard | `tarse` | 116 | 1,050 | 973 | 133 | 31 | 6 | 3.537 | 2,385 |

- The answers that take 7 are the classic hard-mode traps, families that
  differ in one letter: minimax *shave, tight, hatch, mound*; entropy
  *mound, willy, grown, poker, goner, catch*. Recorded, not fixed: the spec
  allows it, and only allowed words can be guessed. Whether to do something
  about it is a later decision.
- The rule is in `src/hard_mode.{hpp,cpp}`: `hardModeConstraints(veterans)`
  (the Green letter fixed at each position, and the minimum copies of each
  letter: the highest count from any one guess, never the sum) and
  `isHardModeLegal`. `getBestGuess` filters the guess list with them only in
  hard mode (§5.12); normal mode ranks the guess list directly.
- `tests/reference/simple_hard_mode.hpp` writes rule B another way: for each
  earlier guess, cross every lit letter out of a copy of the new guess. A
  fast test compares the two on every word in the guess list after six
  histories with repeated letters, and every spec example is a hand test.
- The slow suite now has 4 trees (2 strategies × 2 modes), each pinned with
  its digest; the exact numbers live in `kRecorded` in `full_game_test.cpp`,
  the table above is a copy for reading. Hard-mode walks are much faster than
  normal ones (the pools are small). The walk checks every move with the
  simple rule as it goes (`GameTree::hardModeViolations`, which must be 0 in
  hard mode); a normal-mode walk on a tiny list shows the check can fire.
- `StoredFirstGuess.IsUpToDate` checks the file once per strategy, since the
  first guess is the same in hard mode.
- `EveryAnswerIsSolvedWithinSixGuesses` is now `EveryAnswerIsSolved`: within
  6 is still required in normal mode, and it now also checks that no answer
  is left unsolved at the depth limit.
- Mutation checks, each caught: Greens ignored, counts summed across
  guesses, Yellows not counted, NYT's looser one-copy rule, the hard pool not
  used, the constructor ignoring hard mode, `setHardMode` unable to turn it
  on, `reset()` turning it off, the violation check disabled, the walk
  ignoring hard mode. The `reset()` one was found by the T10 review: before
  T10 the flag wasn't stored, so the spec's "reset keeps the hard-mode
  setting" couldn't be tested; now `ResetKeepsHardModeOn` checks it.

### T11. Benchmark

- `nudge_bench`, built on the same tree walk as the slow tests. For each
  strategy and mode it reports average guesses, worst case, the full
  distribution, and for every turn the candidate count, pool size and time.
- It also times the two defined worst cases from §5.9.
- Output: a readable table and a machine-readable file, attached to the CI
  run.

**Done when:** the slowest turn natively, including the worst cases, is under
500 ms on your laptop.

*Why the benchmark shares the tree walk:* draft 1 played every game twice,
once in the slow tests and once in the benchmark. Sharing the walk means one
implementation and no duplicated work in CI.

**Done in T11: the budget is met natively, with the real strategies.** The
slowest hint is entropy's worst fallback, 335 ms (best of 5, the developer's
Windows laptop, MinGW GCC 12, -O3):

| Worst case (§5.9) | Minimax | Entropy |
|---|---|---|
| Worst opener (`xviii`, 1,549 candidates) | 247 ms | 275 ms |
| Worst fallback (`sughs`, 2,398 candidates) | 317 ms | 335 ms |
| Same in WebAssembly (Node), warm | 329 / 436 ms | 335 / 442 ms |
| Worst fallback in WebAssembly, cold | | 457 ms |

- **Real games are far below the worst cases.** Over every game of both
  strategies in both modes, the slowest hint actually played is 39 ms
  (entropy, turn 2, 226 candidates). The worst cases are positions the
  solver's own openers never reach; they stay as the budget's test because a
  player can open with anything. Turn 1 is 0 ms (the stored first guess),
  and hard-mode turns get cheaper as the pool shrinks (to 15–76 guesses by
  turn 6).
- **WebAssembly is the tight one:** entropy's worst fallback has about 9% to
  spare cold (457 ms). T12 measures it again in its Node tests; Firefox
  (§5.9) is the extension's check. If it goes over, the first fix is still
  the internal one in §5.9.
- `nudge_bench` replaces the T4 spike (`nudge_spike` is deleted): no
  arguments times the two worst cases with both strategies; `--games
  [report.json]` also plays all four trees and prints, per turn, the
  positions, the largest candidate count, the largest pool, and the mean and
  slowest `getBestGuess` time; `<strategy> <opener> <pattern>
  answers|guesses` times one cold hint. It exits with 1 above twice the
  budget, which is CI's loose gate (§8.5).
- The numbers per turn come from the tree walk itself (`GameTree::turns`),
  so the slow tests and the benchmark share one walk; fast tests check the
  counts on the hand-worked tiny trees, in both modes.
- CI: the `gcc` job runs the benchmark after the slow tests and attaches
  `bench-report.json` to the run (the worst cases, then every tree with its
  per-turn numbers); the `wasm` job runs the worst cases warm
  and cold, for both strategies.

### T12. WebAssembly build

- `bindings.cpp` (registration only), the build flags, and Node tests:
  - create a solver, play a few games, check results;
  - every bad input from JavaScript returns the right status text;
  - **native and WebAssembly agree on every position** of the full tree for
    both strategies in normal mode (§8.4);
  - the worst-case positions stay within the budget (§8.5).

**Done when:** the Node tests pass locally and in CI.

*Why last:* WebAssembly only changes how the solver is packaged. With the
spike in T4 done, the remaining risk is small, and every correctness
question has already been answered natively, where debugging is easier.

---

## 8. Testing strategy

| Kind | Examples | When it runs | Label |
|---|---|---|---|
| Unit | feedback cases, input parsing, tie-break, strategy scores, statuses, solver state | after every change, in CI | `fast` |
| Exhaustive and property | feedback vs. the simple version (3-letter alphabet, random pairs) | after every change, in CI | `fast` |
| Full game | every answer, both strategies, both modes; first-guess check | before a commit, in CI | `slow` |
| Determinism | fresh solvers give the same guess | before a commit, in CI | `slow` |
| Sanitizer | fast tests with AddressSanitizer and UBSan | in CI | `fast` |
| WebAssembly | Node tests; native vs. WebAssembly on every position | in CI, locally after T12 | Node |
| Coverage | gcovr over the fast tests; pytest-cov over the script tests | every push, locally before a commit | gate |
| Benchmark | statistics and timing | by hand, and reported in CI | report |

### 8.1 Independent checkers

A test that checks code using that same code can't catch its bugs. Two
places in draft 1 did that; draft 2 fixes both:

- **Full games:** the simulated game decides the feedback for each guess with
  the simple string version (`tests/reference/simple_feedback`), not the
  solver's fast one. If the fast version had a bug, the solver and the
  simulated game would now disagree, and the game would fail.
- **Hard mode:** "every suggestion is allowed" is checked with
  `tests/reference/simple_hard_mode`, written from the spec's wording,
  not with `isHardModeLegal`, which built the pool.

*Why these two:* feedback and hard mode are the two rules copied from the
real game. They're the most likely places for a subtle misunderstanding,
and the full-game tests are what catch it.

### 8.2 Full games as a tree walk

The solver is deterministic, so all 2,309 games together form a tree: every
game starts with the same first guess, games that got the same pattern share
the same second guess, and so on. The test helper walks it depth-first:

```
walk(veteran words, possible answers):
    guess = a solver replayed with these veteran words → getBestGuess()
    group the possible answers by simple_feedback(guess, answer)
    for each group:
        all green → record the game length for that answer
        otherwise → walk(veteran words + (guess, pattern), that group)
```

- Each position is computed once, so there's no cache and nothing to get
  wrong in a cache key.
- A fresh solver is built for each position by replaying the veteran words.
  Replaying only costs a few `derive` calls, and it means each position is
  computed exactly as the extension would compute it.
- A depth limit (10) stops the walk if a bug made it loop.
- The distribution, worst case and slowest position fall out of the walk
  directly, which is why the benchmark (T11) reuses it.

### 8.3 Determinism

Determinism tests build **fresh solvers**; draft 1 would have answered from a
cache, which proves nothing. As built: a fast test asks 10 new solvers for the
same position on a tiny list (T6), and the slow suite walks the whole real
tree a second time with new solvers at every position and compares the two
(T7). Ten full walks would cost about a minute for little extra. Both run in
one process, so a different-process check comes from §8.4 (native versus
WebAssembly) and T9's stored first guess.

### 8.4 Native and WebAssembly agree

`nudge_tree` (native) writes the whole tree (each position and its guess) for
both strategies. The Node test walks the same tree with the WebAssembly
solver and compares every position. In CI, the `wasm` job builds both and
generates the native file fresh on every run, so there's no committed file
to go stale.

*Why the whole tree instead of 50 games:* the risk is a rare near-tie where
the two math libraries round differently (§5.7). A sample of 50 games would
very likely miss it, and the whole tree is only a few thousand positions.

### 8.5 Timing

- **Locally (strict):** the benchmark's worst cases must stay under 500 ms on
  your laptop, natively and in WebAssembly. This is the real check.
- **In CI (report, loose gate):** times are reported on every run. CI only
  fails if a worst case exceeds 2× the budget, because GitHub's shared
  machines vary too much in speed for a strict limit.

*Why the budget is 500 ms and not the full second:* the extension also has to
read the board, send it to the solver and draw the hint. The end-to-end
1-second check belongs to the extension's plan.

### 8.6 100% code coverage

**The rule:** every line and every function of the solver library
(`solver/src/`, `solver/include/`) and of the import script is run by at
least one test. CI fails below 100%, from T1 onward.

| Code | Measured? | Why |
|---|---|---|
| `solver/src/`, `solver/include/` | **Yes, 100%** | The code the specs describe. |
| `scripts/import_word_lists.py` | **Yes, 100%** except the download function | It decides what data the solver uses. |
| `data/*.inc` | No | Data, not code. The script that writes it is covered. |
| `solver/tests/` (including `reference/`) | No | Tests aren't measured against themselves. |
| `solver/bench/`, `solver/tools/` | No, but each has a smoke test | Developer programs, not shipped code. Each is run on a tiny list to check it works. |
| `solver/wasm/bindings.cpp` | Not by gcovr | gcovr can't measure WebAssembly. It only registers functions (§5.15); everything it calls is in the core and covered. The Node tests call each registered function. |

*Why the fast tests are enough:* they run on tiny, hand-built lists, so they
can reach every path: fallback, impossible state, all-green, every tie-break
rule, every status. If a line is reached only by the slow tests, a fast test
is missing.

*Why a separate coverage build:* coverage needs Debug, no optimization and
extra flags. Its own folder (`build-cov`) keeps the Release build and the
benchmark unaffected.

*Lines and functions gated; branches reported:* draft 1's reason for not
gating branches was wrong (branches inside standard-library code are filtered
out, since they're attributed to library headers). The real question is
whether exception-handling branches the compiler adds to your own lines
remain after `--exclude-throw-branches`. T2 measures the actual branch
number; if it's clean, branch coverage gets gated at 100% too.

*Known friction, handled in T1 and T6:* compilers generate several versions of
a virtual destructor, and coverage tools sometimes report an unused one as a
function that never ran. Defining the destructor once in a `.cpp` file (§5.6)
avoids most of it. Anything left gets a commented exclusion.

*Python coverage scope:* `scripts/.coveragerc` leaves out `scripts/tests/`
and the `if __name__ == "__main__":` line, which coverage.py would otherwise
count.

*What 100% coverage does not prove:* that a test **checked** anything. A test
with no assertions still counts. Coverage finds code no test touches;
correctness comes from the hand-checked cases, the independent checkers and
the full-game tests. That's also why §8.7 exists.

*Exclusion markers:* each exclusion needs a comment on the same line saying
why, and should be rare. Defensive code that "can never happen" is usually
better removed or turned into an assertion than excluded.

### 8.7 Mutation checks

In T2 and T6, make one deliberate bug (drop the `counts -= 1` step; flip
tie-break rule 2) and confirm a test fails, then undo it. Record the result in
the commit message.

*Why:* it's a five-minute check that the tests actually look at results,
which coverage alone can't show.

---

## 9. CI (GitHub Actions)

All jobs run on Linux (`ubuntu-latest`), on every push and pull request.
Every action and tool is pinned to a fixed version, so an upstream release
can't break CI unexpectedly.

| Job | What it does | Added in |
|---|---|---|
| `gcc` | GCC Release build, fast tests, then slow tests | T1 (slow from T7) |
| `clang` | Clang Release build, fast tests | T1 |
| `sanitizers` | Clang Debug build with `-fsanitize=address,undefined`, fast tests | T2 |
| `lint` | clang-format check on all solver files; clang-tidy on the library sources (`solver/src`), using the exported compile commands | T1 |
| `coverage` | Coverage build, fast tests, gcovr gate at 100%; uploads the report | T1 |
| `python` | pytest with the coverage gate at 100% | T3 |
| `wasm` | emsdk and Node 24, WebAssembly build; from T12: native tree dump plus Node tests | T4 (tests from T12) |
| `bench` | Runs `nudge_bench`; reports times; fails only above 2× the budget | T11 |

*Why Linux when you develop on Windows:* AddressSanitizer (which catches memory
bugs like reading past the end of an array) isn't available with MinGW GCC.
Checked in T1: MinGW GCC 12.2 ships neither `libasan` nor `libubsan`, so both
sanitizers only run in CI.
CI runs it on every push without changing your setup.

*Why a Clang job:* your machine and the `gcc` job both use GCC. Clang is a
different compiler with different warnings, so code that only one of them
accepts gets caught immediately.

*Why sanitizers only run the fast tests:* sanitizers slow code down several
times. The fast tests already reach every line; the slow tests mostly repeat
the same paths thousands of times.

---

## 10. Risks

| # | Risk | How likely | What happens |
|---|---|---|---|
| R1 | A strategy can't solve some answer within 6 guesses in normal mode | Low to medium | A spec must-have fails. Found in T7/T8. The specs rule out lookahead, so the fix is your decision: change the requirement, or approve a specific fix in the spec. |
| R2 | WebAssembly is too slow for the 500 ms budget | Medium | Found in T4, before the scoring loop is built. Options in §5.9. |
| R3 | Early fallback is too slow | Medium | Found in T4. Options in §5.9; the spec requires 1 second on every turn, so accepting a slower fallback hint is a spec decision. |
| R4 | NYT's hard mode is looser than rule B | Medium | Harmless: the solver just skips a few guesses NYT would allow (§5.12). If NYT were ever stricter than rule B, only `isHardModeLegal` changes. |
| R5 | MinGW on Windows and GCC/Clang on Linux behave differently | Low | CI builds with both GCC and Clang on every push. |
| R6 | Installing Emscripten on Windows is fiddly | Medium | Done in T4, early, instead of at the end. |
| R7 | The real answer is missing from the answer list while other list words still fit | Low per game | The solver narrows the wrong set until that's ruled out (§5.13). Documented and pinned by a test; a fix would be a spec decision. |

---

## 11. Changes to the specs

### Already made (your decisions)

1. All green always means solved.
2. Words are accepted in any case and stored and returned lowercase.
3. `setStrategy` returns `UnknownStrategy` for an unknown name or an empty
   pointer; a name-based `setStrategy` was added.
4. Tied = within `1e-9` of the best score, one constant for both strategies.

5. After the game is solved, `addFeedbackPattern` returns `GameOver`.
6. The solver never resets itself; the extension calls `reset()` only when
   the Wordle board is empty.
7. The late-fallback window (R7) is accepted for v1 and documented as a
   flagged concern.

### Proposed by this plan (approved, written into the specs)

| # | Spec said | Spec now says | Why |
|---|---|---|---|
| P1 | `score(guess, candidates)` | `score(bucketCounts, total)` plus `name()` | §5.6 |
| P2 | Constructor `(strategy, hardMode)` | Same, plus an optional `WordLists` argument defaulting to the built-in lists | §5.11 |
| P3 | The first guess belongs to each strategy (`precomputedFirstGuess()`) | First guesses are stored with the word lists; computed when not stored | §5.11 |
| P4 | First guess "computed by a build script" | Computed by a tool you run, committed, checked by a test | The WebAssembly build can't run a native tool (§5.11) |
| P5 | "Two static word-list files ship inside the extension" | The lists are compiled into the solver from `data/*.inc` | §5.10 |
| P6 | "The only feedback implementation in the project" | One production implementation; a separate, simple test-only version is used as a checker | §8.1 |
| P7 | 1 second per hint | Unchanged; the solver's share is 500 ms | §8.5 |
| P8 | `reset()` behavior not fully defined | Clears the game (veteran words, fallback); keeps the strategy and the hard-mode setting | The strategy is the user's choice and hard mode is the game's setting; neither belongs to one game |
| P9 | Hard-mode repeated letters: open | Rule B: counted letters, at least as strict as NYT | §5.12 |
| P10 | The answer list "in the original game's daily order" | Sorted; the daily order isn't used | That phrase described the source; nothing in the spec needs the order |

### Still open

- **The default strategy:** minimax for now; an open question in the
  top-level intent, for the extension.

---

## 12. Handover to the extension plan

Things this plan deliberately leaves for the extension:

- **Where the solver runs.** A hint can take up to several hundred
  milliseconds, so it shouldn't run on the page's main thread. Whether that
  means a Web Worker, a background page, or another extension page depends on
  what Firefox allows from a content script; decide it in the extension plan.
- **Allowing WebAssembly.** Firefox extensions need a content security policy
  that permits compiling WebAssembly (`'wasm-unsafe-eval'`). The exact
  manifest key differs between Manifest V2 and V3; check current Firefox
  documentation once the manifest version is chosen.
- **Loading the `.wasm` file:** as a separate packaged file, or embedded into
  the JavaScript with `-sSINGLE_FILE`.
- **What to show** in fallback mode, on `ImpossibleState` (with the GitHub
  issues link), and on `UnknownStrategy` (for example, reset the saved
  setting to the default).
- **Detecting hard mode** on the NYT page and calling `setHardMode`.
- **Starting a new game:** call `reset()` only when the Wordle board on the
  page is empty. The solver never resets itself.
- **The end-to-end 1-second measurement.**
- **The default strategy** (minimax for now; an open question in the
  top-level intent).

---

## 13. Definition of done

- Every acceptance criterion in the three specs passes in CI (section 14).
- The feedback rule is checked once against the original game's code.
- Line and function coverage is 100% for the solver library and the import
  script, enforced in CI, with every exclusion commented.
- The benchmark report exists for both strategies in both modes, including the
  defined worst cases.
- The WebAssembly module works from JavaScript, matches native on every
  position of the tree, and stays within its 500 ms budget locally.
- `CLAUDE.md` lists the build, test, coverage and benchmark commands.

---

## 14. Acceptance criteria → tasks

Every acceptance criterion from the specs, and the task that proves it.

**Shared spec**

| Acceptance criterion | Task |
|---|---|
| `computeFeedback` matches the four hand-checked cases | T2 |
| Tie-break: one test per rule, including one only rule 3 resolves | T6 |
| Same input 10 times gives the same guess | T7, T8 (fresh solvers, §8.3) |
| Hard mode: every suggestion allowed, across every answer | T10 (simple checker, §8.1) |
| Every answer is in the guess list | T3 |
| A 5-letter word not in the list is accepted; bad words return `InvalidWord` | T2, T5 |
| An answer missing from the answer list switches to fallback and is still solved | T5 |
| `ImpossibleState` leaves count, veteran words and fallback unchanged | T5 |
| All-green for a word in neither list returns `Ok`, then that word | T5, T6 |
| After the game is solved, `addFeedbackPattern` returns `GameOver` | T5 |
| `reset()` clears the game, keeps strategy and hard mode | T5, T10 (hard mode) |
| `"CRANE"`, `"Crane"`, `"crane"` all stored as `crane` | T2, T5 |
| `setStrategy`: empty pointer or unknown name → `UnknownStrategy`; known names → `Ok` | T5, T8 |
| Stored first guess matches a fresh recomputation | T9 |
| Slowest turn under 1 second | T4, T11, T12 (solver's 500 ms); end to end in the extension plan |

**Behaviors the spec describes, with no acceptance criterion of their own**

| Behavior | Task |
|---|---|
| Switching strategy mid-game keeps the game | T8 |
| Turning hard mode on or off mid-game keeps the veteran words; off restores the pool | T10 |
| One candidate left returns it | T6 |
| The first guess is the same in hard mode | T9 |
| The same word entered twice | T5 |
| Hard mode together with fallback | T10 |

**Knuth/minimax spec**

| Acceptance criterion | Task |
|---|---|
| Hand-checked worst-case bucket sizes on a small set | T7 |
| A case where the best guess is not a candidate | T7 |
| Every answer within 6 guesses in normal mode | T7 |
| Hard-mode distribution and number over 6 recorded | T10, T11 |
| Average, worst case and distribution recorded | T11 |

**Entropy spec**

| Acceptance criterion | Task |
|---|---|
| Hand-checked scores, including 2 bits and 0 bits | T8 |
| A case where the best guess is not a candidate | T8 |
| Two guesses tied within the tolerance, resolved by the tie-break | T6 (via `pickWinner`), T8 |
| Every answer within 6 guesses in normal mode | T8 |
| Hard-mode distribution and number over 6 recorded | T10, T11 |
| Results in the same format as minimax | T11 |
