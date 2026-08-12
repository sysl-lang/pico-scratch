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

# **A package's C is compiled whole, whatever the program imports** — so this project pays for the
# radio it never mentions. `pico2` v0.0.6 put four C files in the package (`scan.c`, `dns.c`, `net.c`,
# `http.c`), and `sysl build-c` walks the dependency's tree and compiles every one of them into the
# archive. Nothing here calls `scan` or `fetch`; the objects go in unreferenced and the linker drops
# them, but they still have to *compile*, and they include `pico/cyw43_arch.h` and `lwip/dns.h`.
#
# That is why this file grew an include block it did not have while the package was pinned at 0.0.4.
# The alternative is not upgrading, and 0.0.7 is the tag where the line editor moved into the
# standard library — so the choice was between paying for the radio's headers and keeping two hundred
# lines of editor on the board.
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-sysroot
    OUTPUT_VARIABLE ARM_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

# The executable's own resolved list rather than any one library's, for the reason `wifi/` gives at
# more length: a header two link libraries away is still a header this compile needs, and CMake has
# already worked the closure out for the C side.
set(SYSL_C_INCLUDES
    ${ARM_SYSROOT}/include
    ${CMAKE_CURRENT_LIST_DIR}
    $<TARGET_PROPERTY:sysl_repl,INCLUDE_DIRECTORIES>)

# `pico/cyw43_arch.h` `#error`s rather than choosing a variant, so finding the headers is not enough.
set(SYSL_C_DEFINES $<TARGET_PROPERTY:sysl_repl,COMPILE_DEFINITIONS>)

find_program(SYSL_COMPILER sysl REQUIRED)

add_custom_command(
    OUTPUT ${SYSL_ARCHIVE} ${SYSL_ARCHIVE}.h
    # The board comes from the `pico2` package, declared in `repl/package.hocon` and fetched by
    # coordinate — so there is no path to a sibling checkout here, and this builds from a fresh
    # clone with nothing else present.
    COMMAND ${SYSL_COMPILER} build-c ${CMAKE_CURRENT_SOURCE_DIR}/repl --target thumb-freestanding-softfp
            "$<$<BOOL:${SYSL_C_INCLUDES}>:--include-path;$<JOIN:${SYSL_C_INCLUDES},;--include-path;>>"
            "$<$<BOOL:${SYSL_C_DEFINES}>:-D;$<JOIN:${SYSL_C_DEFINES},;-D;>>"
            -o ${SYSL_ARCHIVE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/repl/repl.sysl
            ${CMAKE_CURRENT_SOURCE_DIR}/repl/package.hocon
            ${SYSL_COMPILER}
    COMMENT "Compiling the sysl module for thumbv8m.main"
    COMMAND_EXPAND_LISTS
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
