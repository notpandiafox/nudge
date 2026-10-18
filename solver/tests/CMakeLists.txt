include(GoogleTest)

# Fast tests: run after every change; coverage is measured on these alone.
add_executable(nudge_fast_tests
    feedback_test.cpp
    minimax_strategy_test.cpp
    ranking_test.cpp
    text_format_test.cpp
    tree_walk_test.cpp
    word_lists_test.cpp
    worst_cases_test.cpp
    word_test.cpp
    wordle_solver_test.cpp
)
# Slow tests: every game of the real lists (plan §8.2), before a commit and in CI.
add_executable(nudge_slow_tests
    full_game_test.cpp
)
foreach(tests nudge_fast_tests nudge_slow_tests)
    # Tests also check internal pieces (src/), benchmark helpers (bench/) and use
    # the reference checkers (tests/).
    target_include_directories(${tests} PRIVATE ../src ../bench .)
    target_link_libraries(${tests} PRIVATE nudge_solver GTest::gtest_main)
    nudge_compile_options(${tests})
endforeach()
gtest_discover_tests(nudge_fast_tests PROPERTIES LABELS fast)
gtest_discover_tests(nudge_slow_tests PROPERTIES LABELS slow)
