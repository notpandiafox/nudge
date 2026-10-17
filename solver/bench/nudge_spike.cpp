// T4 speed check (plan §5.9): times one hint's worth of work for the two
// worst cases, natively and, built with Emscripten, as WebAssembly under Node.
//
//   nudge_spike                               find both worst cases, time each (best of 5)
//   nudge_spike <opener> <pattern> <list>     cold: one hint in a fresh process, the way a
//                                             player's first hint runs; <list> is answers
//                                             or guesses (where the candidates come from)
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "worst_cases.hpp"

namespace {

using nudge::Word;

std::string text(const Word& word) {
    std::string out;
    for (const auto letter : word.letters()) {
        out += static_cast<char>('a' + letter);
    }
    return out;
}

// One hint: bucket counts for every guess in the pool against the candidates.
double timeHint(const nudge::WordLists& lists, const std::vector<Word>& candidates,
                std::uint64_t& checksum) {
    const auto start = std::chrono::steady_clock::now();
    for (const Word& guess : lists.guesses()) {
        const auto counts = nudge::bucketize(guess, candidates);
        checksum += *std::max_element(counts.begin(), counts.end());  // keeps the work
    }
    const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - start;
    return took.count();
}

void report(const char* name, const nudge::WordLists& lists, const nudge::WorstCase& worst) {
    double best = 1e9;
    std::uint64_t checksum = 0;
    for (int run = 0; run < 5; ++run) {
        best = std::min(best, timeHint(lists, worst.candidates, checksum));
    }
    std::printf("%s: %s %d, %zu candidates, best of 5: %.1f ms (checksum %llu)\n", name,
                text(worst.opener).c_str(), worst.pattern, worst.candidates.size(), best,
                static_cast<unsigned long long>(checksum));
}

}  // namespace

int main(int argc, char** argv) {
    const nudge::WordLists& lists = nudge::WordLists::builtin();
    if (argc == 4) {
        const auto opener = Word::parse(argv[1]);
        const std::string_view list = argv[3];
        if (!opener || (list != "answers" && list != "guesses")) {
            std::fprintf(stderr, "usage: nudge_spike <opener> <pattern> answers|guesses\n");
            return 1;
        }
        // Filtering first, like the solver does before scoring; then one hint, timed once.
        const auto words = list == "answers" ? nudge::answerWords(lists) : lists.guesses();
        const auto pattern = static_cast<nudge::PatternCode>(std::atoi(argv[2]));
        const auto candidates = nudge::wordsGiving(*opener, pattern, words);
        std::uint64_t checksum = 0;
        const double took = timeHint(lists, candidates, checksum);
        std::printf("cold: %s %d, %zu candidates, one hint: %.1f ms (checksum %llu)\n", argv[1],
                    pattern, candidates.size(), took, static_cast<unsigned long long>(checksum));
        return 0;
    }
    report("worst opener", lists, nudge::worstOpener(lists));
    report("worst fallback", lists, nudge::worstFallback(lists));
    return 0;
}
