add_library(nudge_solver
    src/text_format.cpp
)
target_include_directories(nudge_solver PUBLIC include)

# Warnings for every nudge target. -ffp-contract=off stops the compiler from
# fusing multiply-and-add differently on different machines, so native and
# WebAssembly entropy scores match (plan §5.7).
function(nudge_compile_options target)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        -ffp-contract=off
        $<$<BOOL:${NUDGE_WARNINGS_AS_ERRORS}>:-Werror>
    )
endfunction()
nudge_compile_options(nudge_solver)

if(NUDGE_COVERAGE)
    target_compile_options(nudge_solver PRIVATE --coverage -O0 -g)
    target_link_options(nudge_solver PUBLIC --coverage)
endif()

if(NUDGE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
