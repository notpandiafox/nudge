// The JavaScript interface (plan §5.15): a Solver class for the extension. It
// only connects names to the core; every check and conversion is in the core
// (§5.3), which the native tests cover. The Node tests call every function here.
#include <emscripten/bind.h>

#include <memory>
#include <string>
#include <string_view>

#include "nudge/minimax_strategy.hpp"
#include "nudge/text_format.hpp"
#include "nudge/wordle_solver.hpp"

namespace {

std::string text(nudge::Status status) { return std::string(nudge::statusToText(status)); }

class Solver {
public:
    // Starts with minimax, so creating a solver can't fail; setStrategy picks another.
    explicit Solver(bool hardMode)
        : solver_(std::make_unique<nudge::MinimaxStrategy>(), hardMode) {}

    std::string setStrategy(const std::string& name) {
        return text(solver_.setStrategy(std::string_view(name)));
    }
    std::string addFeedbackPattern(const std::string& word, const std::string& pattern) {
        return text(solver_.addFeedbackPattern(word, std::string_view(pattern)));
    }
    std::string getBestGuess() const { return solver_.getBestGuess(); }
    std::size_t getRemainingCount() const { return solver_.getRemainingCount(); }
    bool isUsingFallback() const { return solver_.isUsingFallback(); }
    void setHardMode(bool on) { solver_.setHardMode(on); }
    void reset() { solver_.reset(); }

private:
    nudge::WordleSolver solver_;
};

}  // namespace

EMSCRIPTEN_BINDINGS(nudge) {
    emscripten::class_<Solver>("Solver")
        .constructor<bool>()
        .function("setStrategy", &Solver::setStrategy)
        .function("addFeedbackPattern", &Solver::addFeedbackPattern)
        .function("getBestGuess", &Solver::getBestGuess)
        .function("getRemainingCount", &Solver::getRemainingCount)
        .function("isUsingFallback", &Solver::isUsingFallback)
        .function("setHardMode", &Solver::setHardMode)
        .function("reset", &Solver::reset);
}
