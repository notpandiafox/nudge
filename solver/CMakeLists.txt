add_library(nudge_solver
    src/feedback.cpp
    src/text_format.cpp
    src/word.cpp
)
# include/ is the spec's public interface; src/ holds internal headers.
target_include_directories(nudge_solver PUBLIC include PRIVATE src)

# Options for every nudge target. -ffp-contract=off stops the compiler from
# fusing multiply-and-add differently on different machines, so native and
# WebAssembly entropy scores match (plan §5.7). Coverage goes on the tests too:
# they hold their own copies of header inline functions, and the linker may
# keep those copies instead of the library's.
function(nudge_compile_options target)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        -ffp-contract=off
        $<$<BOOL:${NUDGE_WARNINGS_AS_ERRORS}>:-Werror>
        $<$<BOOL:${NUDGE_COVERAGE}>:--coverage -O0 -g>
    )
    target_link_options(${target} PRIVATE $<$<BOOL:${NUDGE_COVERAGE}>:--coverage>)
endfunction()
nudge_compile_options(nudge_solver)

if(NUDGE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
