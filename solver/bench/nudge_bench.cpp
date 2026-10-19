// The benchmark (plan T11, §5.9, §8.5): how fast every hint is, and how well
// each strategy plays. Built natively and, with Emscripten, as WebAssembly.
//
//   nudge_bench                        the two worst cases (§5.9) with both strategies,
//                                      best of 5 runs each
//   nudge_bench --games [report.json]  also plays every game for both strategies in both
//                                      modes: guesses, and per turn the candidates, pool
//                                      and hint time; optionally written as JSON
//   nudge_bench <strategy> <opener> <pattern> answers|guesses
//                                      cold: one hint in a fresh process, the way a player's
//                                      first hint runs; the candidates are the answers or
//                                      the guesses (fallback) that give <pattern>
//
// Exits with 1 when a hint takes more than twice the 500 ms budget, CI's loose gate
// (§8.5). The budget itself is checked by reading the numbers on your laptop.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "nudge/text_format.hpp"
#include "tree_walk.hpp"
#include "worst_cases.hpp"

namespace {

using nudge::Word;

constexpr double kBudgetMs = 500;
constexpr const char* kStrategies[] = {"minimax", "entropy"};

struct TimedHint {
    Word guess;
    double ms;
};

// One hint after the opener, ranked the way the solver ranks it (scoring included).
TimedHint timeHint(const nudge::WordLists& lists, const nudge::WorstCase& worst,
                   std::string_view strategy) {
    const auto scorer = nudge::makeStrategy(strategy);
    const auto start = std::chrono::steady_clock::now();
    const Word guess =
        nudge::rankBest(lists.guesses(), worst.candidates, *scorer, worst.opener.mask());
    const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - start;
    return {guess, took.count()};
}

// Returns the slowest best-of-5 time. Writes the "worst_cases" list when json is set.
double reportWorstCases(const nudge::WordLists& lists, std::FILE* json) {
    double slowest = 0;
    const char* separator = "";
    const std::pair<const char*, nudge::WorstCase> cases[] = {
        {"worst opener", nudge::worstOpener(lists)},
        {"worst fallback", nudge::worstFallback(lists)},
    };
    for (const auto& [name, worst] : cases) {
        std::printf("%s: %s %d, %zu candidates\n", name, worst.opener.text().c_str(), worst.pattern,
                    worst.candidates.size());
        for (const char* strategy : kStrategies) {
            const TimedHint hint = timeHint(lists, worst, strategy);
            double best = hint.ms;
            for (int run = 1; run < 5; ++run) {
                best = std::min(best, timeHint(lists, worst, strategy).ms);
            }
            std::printf("  %s plays %s, best of 5: %.1f ms%s\n", strategy,
                        hint.guess.text().c_str(), best, best > kBudgetMs ? "  OVER BUDGET" : "");
            slowest = std::max(slowest, best);
            if (json != nullptr) {
                std::fprintf(json,
                             "%s\n  {\"case\": \"%s\", \"opener\": \"%s\", \"pattern\": %d, "
                             "\"candidates\": %zu, \"strategy\": \"%s\", \"plays\": \"%s\", "
                             "\"best_of_5_ms\": %.3f}",
                             separator, name, worst.opener.text().c_str(), worst.pattern,
                             worst.candidates.size(), strategy, hint.guess.text().c_str(), best);
                separator = ",";
            }
        }
    }
    return slowest;
}

// Plays every game of each strategy in each mode. Returns the slowest hint.
// Writes the "trees" list when json is set.
double reportGames(const nudge::WordLists& lists, std::FILE* json) {
    double slowest = 0;
    bool firstTree = true;
    for (const bool hardMode : {false, true}) {
        for (const char* strategy : kStrategies) {
            const nudge::GameTree tree = nudge::walkAllGames(
                lists, [strategy] { return nudge::makeStrategy(strategy); }, hardMode);
            const char* mode = hardMode ? "hard" : "normal";
            std::printf(
                "\n%s, %s mode: opens with %s, average %.4f, worst %d, unsolved %zu, "
                "positions %zu\n  guesses:",
                strategy, mode, tree.moves.at("").c_str(), nudge::averageGuesses(tree),
                nudge::worstGuesses(tree), tree.unsolved.size(), tree.moves.size());
            for (const auto& [length, count] : tree.gamesByLength) {
                std::printf(" %d: %d", length, count);
            }
            std::printf("\n  turn  positions  max candidates  max pool  mean ms   max ms\n");
            for (const auto& [turn, stats] : tree.turns) {
                std::printf("  %4d  %9d  %14zu  %8zu  %7.1f  %7.1f%s\n", turn, stats.positions,
                            stats.maxCandidates, stats.maxPool, stats.totalMs / stats.positions,
                            stats.maxMs, stats.maxMs > kBudgetMs ? "  OVER BUDGET" : "");
                slowest = std::max(slowest, stats.maxMs);
            }
            if (json == nullptr) {
                continue;
            }
            std::fprintf(json,
                         "%s\n  {\"strategy\": \"%s\", \"mode\": \"%s\", \"opener\": \"%s\", "
                         "\"average\": %.4f, \"worst\": %d, \"unsolved\": %zu, "
                         "\"positions\": %zu, \"games_by_length\": {",
                         firstTree ? "" : ",", strategy, mode, tree.moves.at("").c_str(),
                         nudge::averageGuesses(tree), nudge::worstGuesses(tree),
                         tree.unsolved.size(), tree.moves.size());
            const char* separator = "";
            for (const auto& [length, count] : tree.gamesByLength) {
                std::fprintf(json, "%s\"%d\": %d", separator, length, count);
                separator = ", ";
            }
            std::fprintf(json, "}, \"turns\": [");
            separator = "";
            for (const auto& [turn, stats] : tree.turns) {
                std::fprintf(json,
                             "%s{\"turn\": %d, \"positions\": %d, \"max_candidates\": %zu, "
                             "\"max_pool\": %zu, \"mean_ms\": %.3f, \"max_ms\": %.3f}",
                             separator, turn, stats.positions, stats.maxCandidates, stats.maxPool,
                             stats.totalMs / stats.positions, stats.maxMs);
                separator = ", ";
            }
            std::fprintf(json, "]}");
            firstTree = false;
        }
    }
    return slowest;
}

int usage() {
    std::fprintf(stderr,
                 "usage: nudge_bench [--games [report.json]]\n"
                 "       nudge_bench minimax|entropy <opener> <pattern> answers|guesses\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    const nudge::WordLists& lists = nudge::WordLists::builtin();
    double slowest = 0;
    if (argc == 5) {
        const auto opener = Word::parse(argv[2]);
        const std::string_view list = argv[4];
        if (!nudge::makeStrategy(argv[1]) || !opener || (list != "answers" && list != "guesses")) {
            return usage();
        }
        const auto pattern = static_cast<nudge::PatternCode>(std::atoi(argv[3]));
        const nudge::WorstCase position{
            *opener, pattern,
            nudge::wordsGiving(*opener, pattern,
                               list == "answers" ? lists.answers() : lists.guesses())};
        const TimedHint hint = timeHint(lists, position, argv[1]);
        slowest = hint.ms;
        std::printf("cold: %s after %s %d, %zu candidates, plays %s, one hint: %.1f ms\n", argv[1],
                    argv[2], pattern, position.candidates.size(), hint.guess.text().c_str(),
                    hint.ms);
    } else if (argc == 1 || (argc <= 3 && std::string_view(argv[1]) == "--games")) {
        // Opened first, so a bad path fails before minutes of work.
        std::FILE* json = argc == 3 ? std::fopen(argv[2], "w") : nullptr;
        if (argc == 3 && json == nullptr) {
            std::fprintf(stderr, "can't write %s\n", argv[2]);
            return 2;
        }
        if (json != nullptr) {
            std::fprintf(json, "{\"budget_ms\": %.0f,\n\"worst_cases\": [", kBudgetMs);
        }
        slowest = reportWorstCases(lists, json);
        if (argc > 1) {
            if (json != nullptr) {
                std::fprintf(json, "\n],\n\"trees\": [");
            }
            slowest = std::max(slowest, reportGames(lists, json));
        }
        if (json != nullptr) {
            std::fprintf(json, "\n]}\n");
            std::fclose(json);
        }
    } else {
        return usage();
    }
    std::printf("\nslowest hint: %.1f ms (budget %.0f ms)\n", slowest, kBudgetMs);
    return slowest > 2 * kBudgetMs ? 1 : 0;
}
