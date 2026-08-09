cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# **No `PICO_HARD_FLOAT_ABI`, and that is what `thumb-freestanding-softfp` is for.** GNU ld refuses
# to merge objects whose float ABIs disagree — "uses VFP register arguments, <output> does not" —
# whether or not a float ever crosses the boundary, so somebody has to move. Until sysl 0.0.35 its
# only Cortex-M33 target was `thumbv8m.main-none-eabihf` and this project set the SDK's switch so the
# C side matched sysl; the SDK's default is `softfp`, so this is a stock pico-sdk now and it is sysl
# that follows.

include(pico_sdk_import.cmake)

project(sysl_repl C CXX ASM)

pico_sdk_init()

# The sysl half, compiled to a static archive and a header by the compiler itself. `build-c` writes
# both, and names the header after the archive.
set(SYSL_ARCHIVE ${CMAKE_CURRENT_BINARY_DIR}/librepl.a)

add_custom_command(
    OUTPUT ${SYSL_ARCHIVE} ${SYSL_ARCHIVE}.h
    # The board comes from the `pico2` package, declared in `repl/package.hocon` and fetched by
    # coordinate — so there is no path to a sibling checkout here, and this builds from a fresh
    # clone with nothing else present.
    COMMAND sysl build-c ${CMAKE_CURRENT_SOURCE_DIR}/repl --target thumb-freestanding-softfp
            -o ${SYSL_ARCHIVE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/repl/repl.sysl
            ${CMAKE_CURRENT_SOURCE_DIR}/repl/package.hocon
    COMMENT "Compiling the sysl module for thumbv8m.main"
    VERBATIM)

add_custom_target(sysl_archive DEPENDS ${SYSL_ARCHIVE})

# There is no C in this project at all. sysl exports `main` itself, so the SDK's `crt0` branches
# straight into it. CMake accepts the archive in the source list and hands it to the linker, which
# satisfies its "a target must have sources" rule without inventing a placeholder file — and with no
# C translation unit there is no language for it to infer, hence `LINKER_LANGUAGE`.
add_executable(sysl_repl ${SYSL_ARCHIVE})

set_target_properties(sysl_repl PROPERTIES LINKER_LANGUAGE C)

add_dependencies(sysl_repl sysl_archive)

target_link_libraries(sysl_repl pico_stdlib pico_cyw43_arch_none ${SYSL_ARCHIVE})

pico_enable_stdio_usb(sysl_repl 1)
pico_enable_stdio_uart(sysl_repl 0)

pico_add_extra_outputs(sysl_repl)
