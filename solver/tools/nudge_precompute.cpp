// Computes each strategy's first guess on the built-in lists (plan §5.11):
//
//   nudge_precompute > data/first_guesses.inc
//
// The WebAssembly build can't run a native tool, so the result is committed;
// the slow test StoredFirstGuessIsUpToDate fails when it is out of date.
#include <cstdio>

#include "nudge/text_format.hpp"
#include "nudge/word_lists.hpp"
#include "ranking.hpp"

int main() {
    const nudge::WordLists& lists = nudge::WordLists::builtin();
    std::puts("// Written by nudge_precompute (solver/tools): the first guess of each strategy");
    std::puts("// on the built-in lists. Rerun it whenever a list or a strategy changes.");
    for (const char* name : {"minimax", "entropy"}) {
        const nudge::Word first =
            nudge::rankBest(lists.guesses(), lists.answers(), *nudge::makeStrategy(name), 0);
        std::printf("{\"%s\", \"%s\"},\n", name, first.text().c_str());
    }
}
