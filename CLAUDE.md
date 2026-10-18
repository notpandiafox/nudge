## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).

## Project

nudge: a Firefox extension that gives hints for NYT games. The Wordle solver is a C++20 library in
`solver/`, compiled to WebAssembly for the extension. What to build is in `docs/` (intent, spec,
plan); work follows the task list in `docs/wordle-solver/plan.md` §7, one task per commit.

## Feedback loop

After every change, build and run the fast tests. Before saying a task is done, also run the
coverage gate (100% line and function coverage of `solver/src` and `solver/include`) and the
formatting check. A task isn't done while any of them fails.

```bash
# Build and fast tests
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DNUDGE_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build -L fast --output-on-failure

# Slow tests (full games), from task T7 on
ctest --test-dir build -L slow --output-on-failure

# Coverage gate (needs: pip install gcovr==8.7). --delete clears old coverage data after each run.
cmake -S . -B build-cov -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNUDGE_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov -L fast
gcovr -r . --object-directory build-cov --filter solver/src/ --filter solver/include/ --fail-under-line 100 --fail-under-function 100 --exclude-throw-branches --exclude-unreachable-branches --exclude-noncode-lines --txt --delete

# Formatting (needs: pip install clang-format==23.1.3)
git ls-files -co --exclude-standard 'solver/*.cpp' 'solver/*.hpp' | xargs clang-format --dry-run --Werror
```

On this Windows machine, CMake and Ninja come from CLion; put them on PATH first:
`C:/Program Files/JetBrains/CLion 2024.1.3/bin/cmake/win/x64/bin` and
`C:/Program Files/JetBrains/CLion 2024.1.3/bin/ninja/win/x64`. If CLion has updated, replace
`2024.1.3` with the new version folder. The compiler is MinGW GCC 12.
graphify isn't on PATH either; run it by its full path:
`C:/Users/oscar/AppData/Local/Python/pythoncore-3.14-64/Scripts/graphify.exe` (for example
`.../graphify.exe update .`). Its output, `graphify-out/`, is ignored through `.git/info/exclude`,
so it stays on this machine and is never committed.
clang-tidy runs in CI (Linux); locally CLion's clang-tidy only works on `solver/src` files, with
`--extra-arg=--target=x86_64-w64-mingw32`.
