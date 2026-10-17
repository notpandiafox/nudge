include(GoogleTest)

# Fast tests: run after every change; coverage is measured on these alone.
add_executable(nudge_fast_tests
    feedback_test.cpp
    text_format_test.cpp
    word_test.cpp
)
# Tests also check internal pieces (src/) and use the reference checkers (tests/).
target_include_directories(nudge_fast_tests PRIVATE ../src .)
target_link_libraries(nudge_fast_tests PRIVATE nudge_solver GTest::gtest_main)
nudge_compile_options(nudge_fast_tests)
gtest_discover_tests(nudge_fast_tests PROPERTIES LABELS fast)
