# The benchmark (plan T11); built natively and with Emscripten. It plays games
# with the same tree walk as the slow tests (tests/tree_walk.hpp).
add_executable(nudge_bench nudge_bench.cpp)
target_include_directories(nudge_bench PRIVATE ../src ../tests)
target_link_libraries(nudge_bench PRIVATE nudge_solver)
nudge_compile_options(nudge_bench)
