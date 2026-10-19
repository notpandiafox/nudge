#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hard_mode.hpp"
#include "nudge/text_format.hpp"
#include "nudge/wordle_solver.hpp"
#include "reference/simple_feedback.hpp"
#include "reference/simple_hard_mode.hpp"

namespace nudge {

// The work of one turn across every game (turn 1 = the first guess), for the
// benchmark (T11): how big the hint was and how long getBestGuess took.
struct TurnStats {
    int positions = 0;              // positions at this turn, each a hint
    std::size_t maxCandidates = 0;  // the most candidates any of them had
    std::size_t maxPool = 0;        // the most guesses any of them could choose from
    double totalMs = 0;             // getBestGuess time, all positions
    double maxMs = 0;               // getBestGuess time, the slowest position
};

// Every game at once, played as a decision tree (plan §8.2). The solver is
// deterministic, so games that got the same colors so far share the next
// guess: each position is computed once, by a fresh solver. The colors come
// from the independent reference feedback, never the solver's own, so a bug
// in the solver's feedback makes games fail instead of hiding.
struct GameTree {
    std::map<int, int> gamesByLength;          // guesses needed -> number of answers
    std::vector<std::string> unsolved;         // answers still open at the depth limit
    std::map<std::string, std::string> moves;  // position -> the guess made there
    int countMismatches = 0;  // positions where the solver's candidate count or fallback disagrees
    int replayFailures = 0;   // veteran words the solver refused while replaying a position
    // Moves hard mode forbids, by the independent rule (plan §8.1). Counted in every
    // walk, so a normal-mode walk shows the check can fire; must be 0 in hard mode.
    int hardModeViolations = 0;
    std::map<int, TurnStats> turns;  // turn number -> its work
};

// Pattern text as the reference checkers read it, e.g. "GYXXG".
inline std::string patternText(const FeedbackPattern& pattern) {
    std::string text;
    for (const Tile tile : pattern) {
        text += "XYG"[static_cast<int>(tile)];
    }
    return text;
}

using MakeStrategy = std::function<std::unique_ptr<ScoringStrategy>()>;

inline constexpr int kDepthLimit = 10;  // stops the walk if a bug makes it loop

inline void walkFrom(const WordLists& lists, const MakeStrategy& makeStrategy,
                     std::vector<VeteranWord>& veterans, const std::string& position,
                     const std::vector<std::string>& answers, GameTree& tree, bool hardMode) {
    WordleSolver solver(makeStrategy(), hardMode, lists);
    for (const VeteranWord& veteran : veterans) {
        if (solver.addFeedbackPattern(veteran.word, veteran.pattern) != Status::Ok) {
            ++tree.replayFailures;
        }
    }
    // A count check, not a word-by-word one (the solver exposes no candidate
    // list). Fallback can never be right here: the true answer is always in the list.
    if (solver.getRemainingCount() != answers.size() || solver.isUsingFallback()) {
        ++tree.countMismatches;
    }
    const auto start = std::chrono::steady_clock::now();
    const std::string guess = solver.getBestGuess();
    const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - start;
    tree.moves[position] = guess;

    const int guessesSoFar = static_cast<int>(veterans.size()) + 1;
    TurnStats& stats = tree.turns[guessesSoFar];
    ++stats.positions;
    stats.maxCandidates = std::max(stats.maxCandidates, solver.getRemainingCount());
    std::size_t pool = lists.guesses().size();
    if (hardMode) {
        const HardModeConstraints constraints = hardModeConstraints(veterans);
        pool = static_cast<std::size_t>(
            std::count_if(lists.guesses().begin(), lists.guesses().end(),
                          [&](const Word& word) { return isHardModeLegal(word, constraints); }));
    }
    stats.maxPool = std::max(stats.maxPool, pool);
    stats.totalMs += took.count();
    stats.maxMs = std::max(stats.maxMs, took.count());
    std::vector<std::pair<std::string, std::string>> history;
    for (const VeteranWord& veteran : veterans) {
        history.emplace_back(veteran.word, patternText(veteran.pattern));
    }
    if (!reference::simpleHardModeAllows(history, guess)) {
        ++tree.hardModeViolations;
    }

    std::map<std::string, std::vector<std::string>> groups;  // reference pattern -> answers
    for (const std::string& answer : answers) {
        groups[reference::simpleFeedback(guess, answer)].push_back(answer);
    }
    for (const auto& [pattern, group] : groups) {
        if (pattern == "GGGGG") {
            ++tree.gamesByLength[guessesSoFar];
        } else if (guessesSoFar >= kDepthLimit) {
            tree.unsolved.insert(tree.unsolved.end(), group.begin(), group.end());
        } else {
            veterans.push_back({guess, *parsePattern(pattern)});
            walkFrom(lists, makeStrategy, veterans, position + guess + ":" + pattern + " ", group,
                     tree, hardMode);
            veterans.pop_back();
        }
    }
}

// A checksum of every move, the same on every platform (64-bit FNV-1a; std::hash
// differs between standard libraries). Pinning it pins the whole tree, not just its shape.
inline std::uint64_t movesDigest(const std::map<std::string, std::string>& moves) {
    std::uint64_t hash = 0xcbf29ce484222325u;
    const auto add = [&hash](std::string_view text) {
        for (const char c : text) {
            hash = (hash ^ static_cast<unsigned char>(c)) * 0x100000001b3u;
        }
    };
    for (const auto& [position, guess] : moves) {
        add(position);
        add("=");
        add(guess);
        add("\n");
    }
    return hash;
}

// Average guesses over all solved games; 0 if none was solved.
inline double averageGuesses(const GameTree& tree) {
    int games = 0;
    int guesses = 0;
    for (const auto& [length, count] : tree.gamesByLength) {
        games += count;
        guesses += length * count;
    }
    return games == 0 ? 0 : static_cast<double>(guesses) / games;
}

// The most guesses any solved game needed; 0 if none was solved.
inline int worstGuesses(const GameTree& tree) {
    return tree.gamesByLength.empty() ? 0 : tree.gamesByLength.rbegin()->first;
}

inline GameTree walkAllGames(const WordLists& lists, const MakeStrategy& makeStrategy,
                             bool hardMode = false) {
    std::vector<std::string> answers;
    for (const Word& answer : lists.answers()) {
        answers.push_back(answer.text());
    }
    GameTree tree;
    std::vector<VeteranWord> veterans;
    walkFrom(lists, makeStrategy, veterans, "", answers, tree, hardMode);
    return tree;
}

}  // namespace nudge
