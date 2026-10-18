#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "nudge/text_format.hpp"
#include "nudge/wordle_solver.hpp"
#include "reference/simple_feedback.hpp"

namespace nudge {

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
};

using MakeStrategy = std::function<std::unique_ptr<ScoringStrategy>()>;

inline constexpr int kDepthLimit = 10;  // stops the walk if a bug makes it loop

inline void walkFrom(const WordLists& lists, const MakeStrategy& makeStrategy,
                     std::vector<VeteranWord>& veterans, const std::string& position,
                     const std::vector<std::string>& answers, GameTree& tree) {
    WordleSolver solver(makeStrategy(), false, lists);
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
    const std::string guess = solver.getBestGuess();
    tree.moves[position] = guess;

    std::map<std::string, std::vector<std::string>> groups;  // reference pattern -> answers
    for (const std::string& answer : answers) {
        groups[reference::simpleFeedback(guess, answer)].push_back(answer);
    }
    const int guessesSoFar = static_cast<int>(veterans.size()) + 1;
    for (const auto& [pattern, group] : groups) {
        if (pattern == "GGGGG") {
            ++tree.gamesByLength[guessesSoFar];
        } else if (guessesSoFar >= kDepthLimit) {
            tree.unsolved.insert(tree.unsolved.end(), group.begin(), group.end());
        } else {
            veterans.push_back({guess, *parsePattern(pattern)});
            walkFrom(lists, makeStrategy, veterans, position + guess + ":" + pattern + " ", group,
                     tree);
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

inline GameTree walkAllGames(const WordLists& lists, const MakeStrategy& makeStrategy) {
    std::vector<std::string> answers;
    for (const Word& answer : lists.answers()) {
        answers.push_back(answer.text());
    }
    GameTree tree;
    std::vector<VeteranWord> veterans;
    walkFrom(lists, makeStrategy, veterans, "", answers, tree);
    return tree;
}

}  // namespace nudge
