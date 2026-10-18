# Native tools you run by hand; their output is committed (plan §5.11).
add_executable(nudge_precompute nudge_precompute.cpp)
target_include_directories(nudge_precompute PRIVATE ../src)
target_link_libraries(nudge_precompute PRIVATE nudge_solver)
nudge_compile_options(nudge_precompute)
