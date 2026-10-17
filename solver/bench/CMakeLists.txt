# T4 speed check; built natively and with Emscripten (plan §5.9).
add_executable(nudge_spike nudge_spike.cpp)
target_include_directories(nudge_spike PRIVATE ../src)
target_link_libraries(nudge_spike PRIVATE nudge_solver)
nudge_compile_options(nudge_spike)
