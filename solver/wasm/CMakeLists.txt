# The JavaScript module the extension imports (plan §5.15): nudge.mjs plus
# nudge.wasm. MODULARIZE and EXPORT_ES6 make it a normal ES module whose
# default export creates the module, instead of a script that sets globals.
# ALLOW_MEMORY_GROWTH: a Solver that JavaScript forgets to delete() costs about
# 100 KB instead of, after about 156 of them, running a fixed 16 MB heap out.
# DYNAMIC_EXECUTION=0: embind otherwise builds functions with new Function(),
# which an extension's content security policy blocks (the extension spike).
add_executable(nudge_wasm bindings.cpp)
target_link_libraries(nudge_wasm PRIVATE nudge_solver)
target_link_options(nudge_wasm PRIVATE -lembind -sMODULARIZE=1 -sEXPORT_ES6=1
                    -sALLOW_MEMORY_GROWTH=1 -sDYNAMIC_EXECUTION=0)
set_target_properties(nudge_wasm PROPERTIES OUTPUT_NAME nudge SUFFIX .mjs)
nudge_compile_options(nudge_wasm)
