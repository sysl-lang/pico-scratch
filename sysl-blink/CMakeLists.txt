cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# **No `PICO_HARD_FLOAT_ABI`, and that is what `thumb-freestanding-softfp` is for.** GNU ld refuses
# to merge objects whose float ABIs disagree — "uses VFP register arguments, <output> does not" —
# whether or not a float ever crosses the boundary, so somebody has to move. Until sysl 0.0.35 its
# only Cortex-M33 target was `thumbv8m.main-none-eabihf` and this project set the SDK's switch so the
# C side matched sysl; the SDK's default is `softfp`, so this is a stock pico-sdk now and it is sysl
# that follows.

include(pico_sdk_import.cmake)

project(sysl_blink C CXX ASM)

pico_sdk_init()

# The sysl half, compiled to a static archive and a header by the compiler itself. `build-c` writes
# both, and names the header after the archive.
set(SYSL_ARCHIVE ${CMAKE_CURRENT_BINARY_DIR}/libblink.a)

# **A package's C is compiled whole, whatever the program imports.** `pico2` has carried four C files
# for the radio since v0.0.6 — `scan.c`, `dns.c`, `net.c`, `http.c` — and `sysl build-c` walks the
# dependency's tree and compiles every one of them into the archive. This program says nothing about
# the radio and still pays for its headers: `dns.c` includes `pico/cyw43_arch.h` and `lwip/dns.h`, and
# without the block below the build stops inside a header on a project whose own code is fine.
#
# **What it does not cost is the link.** The four objects go in unreferenced and the linker leaves
# them there, so this executable carries no lwIP at all.
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-sysroot
    OUTPUT_VARIABLE ARM_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

# The libc half is asked of the toolchain rather than spelled out: `-print-sysroot` answers with the
# directory whose `include` holds newlib's headers, and the obvious path one level up has no
# `string.h` in it and fails exactly as passing nothing does.
#
# The SDK half is the executable's own resolved list rather than any one library's. `cyw43.h` includes
# `lwip/netif.h`, which is two link libraries away from where `scan.c` sits, so naming targets one at
# a time gets each one's headers and none of what they pull in. It names a target defined further
# down, which is fine — a generator expression is resolved after the whole file has been read.
set(SYSL_C_INCLUDES
    ${ARM_SYSROOT}/include
    $<TARGET_PROPERTY:sysl_blink,INCLUDE_DIRECTORIES>)

# And the macros, which finding the headers does not supply: `pico/cyw43_arch.h` `#error`s rather
# than picking an architecture variant, so every path can be right and the compile still stops.
set(SYSL_C_DEFINES $<TARGET_PROPERTY:sysl_blink,COMPILE_DEFINITIONS>)

# Named rather than left to `PATH`, and in `DEPENDS`, because `find_program` is cached: a project
# configured against one `sysl` goes on using it after an upgrade, and what that looks like is a
# language feature that shipped and does not work.
find_program(SYSL_COMPILER sysl REQUIRED)

add_custom_command(
    OUTPUT ${SYSL_ARCHIVE} ${SYSL_ARCHIVE}.h
    # The board comes from the `pico2` package, declared in `blink/package.hocon` and fetched by
    # coordinate — so there is no path to a sibling checkout here, and this builds from a fresh
    # clone with nothing else present.
    COMMAND ${SYSL_COMPILER} build-c ${CMAKE_CURRENT_SOURCE_DIR}/blink --target thumb-freestanding-softfp
            "$<$<BOOL:${SYSL_C_INCLUDES}>:--include-path;$<JOIN:${SYSL_C_INCLUDES},;--include-path;>>"
            "$<$<BOOL:${SYSL_C_DEFINES}>:-D;$<JOIN:${SYSL_C_DEFINES},;-D;>>"
            -o ${SYSL_ARCHIVE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/blink/blink.sysl
            ${CMAKE_CURRENT_SOURCE_DIR}/blink/package.hocon
            ${SYSL_COMPILER}
    COMMENT "Compiling the sysl module for thumbv8m.main"
    COMMAND_EXPAND_LISTS
    VERBATIM)

add_custom_target(sysl_archive DEPENDS ${SYSL_ARCHIVE})

# There is no C in this project at all. sysl exports `main` itself, so the SDK's `crt0` branches
# straight into it. CMake accepts the archive in the source list and hands it to the linker, which
# satisfies its "a target must have sources" rule without inventing a placeholder file — and with no
# C translation unit there is no language for it to infer, hence `LINKER_LANGUAGE`.
add_executable(sysl_blink ${SYSL_ARCHIVE})

set_target_properties(sysl_blink PROPERTIES LINKER_LANGUAGE C)

add_dependencies(sysl_blink sysl_archive)

target_link_libraries(sysl_blink pico_stdlib pico_cyw43_arch_none ${SYSL_ARCHIVE})

pico_enable_stdio_usb(sysl_blink 1)
pico_enable_stdio_uart(sysl_blink 0)

pico_add_extra_outputs(sysl_blink)
