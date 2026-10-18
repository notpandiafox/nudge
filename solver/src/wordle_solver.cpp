#include "nudge/wordle_solver.hpp"

#include <algorithm>
#include <cassert>

#include "feedback_code.hpp"
#include "nudge/text_format.hpp"
#include "ranking.hpp"

namespace nudge {
namespace {

bool isAllGreen(const FeedbackPattern& pattern) {
    return std::all_of(pattern.begin(), pattern.end(),
                       [](Tile tile) { return tile == Tile::Green; });
}

bool isColor(Tile tile) {
    return tile == Tile::Gray || tile == Tile::Yellow || tile == Tile::Green;
}

// A veteran word as the feedback calculation sees it: the guess and its code.
struct Clue {
    Word guess;
    PatternCode code;
};

// The words that would have produced every clue, had they been the answer.
std::vector<Word> consistentWords(const std::vector<Clue>& clues, const std::vector<Word>& words) {
    std::vector<Word> result;
    for (const Word& word : words) {
        const bool fits = std::all_of(clues.begin(), clues.end(), [&](const Clue& clue) {
            return feedbackCode(clue.guess, word) == clue.code;
        });
        if (fits) {
            result.push_back(word);
        }
    }
    return result;
}

}  // namespace

ScoringStrategy::~ScoringStrategy() = default;

WordleSolver::WordleSolver(std::unique_ptr<ScoringStrategy> strategy, bool /*hardMode*/,
                           WordLists lists)
    : lists_(std::move(lists)), strategy_(std::move(strategy)), view_(derive({})) {
    assert(strategy_ && "WordleSolver needs a strategy");
}

Status WordleSolver::addFeedbackPattern(const std::string& word, const FeedbackPattern& pattern) {
    if (view_.solved) {
        return Status::GameOver;
    }
    const auto parsed = Word::parse(word);
    if (!parsed) {
        return Status::InvalidWord;
    }
    if (!std::all_of(pattern.begin(), pattern.end(), isColor)) {
        return Status::InvalidPattern;
    }
    // Build the new state on the side and keep it only if it's possible, so a
    // rejected call changes nothing.
    std::vector<VeteranWord> veterans = veterans_;
    veterans.push_back({parsed->text(), pattern});
    View view = derive(veterans);
    if (!view.solved && view.candidates.empty()) {
        return Status::ImpossibleState;
    }
    veterans_ = std::move(veterans);
    view_ = std::move(view);
    return Status::Ok;
}

std::string WordleSolver::getBestGuess() const {
    if (view_.solved) {
        return view_.candidates.front().text();
    }
    // The first guess depends only on the lists and the strategy, so it can be
    // stored ahead of time (plan §5.11); that keeps turn 1 instant.
    if (veterans_.empty()) {
        if (const auto stored = lists_.firstGuess(strategy_->name())) {
            return stored->text();
        }
    }
    return rankBest(lists_.guesses(), view_.candidates, *strategy_, view_.triedLetters).text();
}

std::size_t WordleSolver::getRemainingCount() const { return view_.candidates.size(); }

const std::vector<VeteranWord>& WordleSolver::getVeteranWords() const { return veterans_; }

bool WordleSolver::isUsingFallback() const { return view_.fallback; }

Status WordleSolver::setStrategy(std::unique_ptr<ScoringStrategy> strategy) {
    if (!strategy) {
        return Status::UnknownStrategy;
    }
    strategy_ = std::move(strategy);
    return Status::Ok;
}

Status WordleSolver::setStrategy(std::string_view name) { return setStrategy(makeStrategy(name)); }

void WordleSolver::reset() {
    veterans_.clear();
    view_ = derive(veterans_);
}

WordleSolver::View WordleSolver::derive(std::span<const VeteranWord> veterans) const {
    std::vector<Clue> clues;
    for (const VeteranWord& veteran : veterans) {
        // Only addFeedbackPattern adds veteran words, after checking them, so parse() can't fail.
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        clues.push_back({*Word::parse(veteran.word), encode(veteran.pattern)});
    }
    View view;
    for (const Clue& clue : clues) {
        view.triedLetters |= clue.guess.mask();
    }
    if (!veterans.empty() && isAllGreen(veterans.back().pattern)) {
        // Solved, even for a word in neither list (spec: all green always means solved).
        view.solved = true;
        view.candidates = {clues.back().guess};
        return view;
    }
    view.candidates = consistentWords(clues, lists_.answers());
    if (view.candidates.empty()) {
        // Adding clues only removes words, so once no answer fits, none ever will
        // again this game: fallback lasts until reset() without a stored flag.
        view.fallback = true;
        view.candidates = consistentWords(clues, lists_.guesses());
    }
    return view;
}

}  // namespace nudge
