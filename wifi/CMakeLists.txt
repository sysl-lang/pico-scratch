cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# sysl's only Cortex-M33 target is `thumbv8m.main-none-eabihf`, which passes floating-point
# arguments in VFP registers. The SDK defaults to `softfp`, which passes them in integer registers,
# and GNU ld refuses to merge objects that disagree — "uses VFP register arguments, <output> does
# not" — whether or not any float ever crosses the boundary. `PICO_HARD_FLOAT_ABI` is the SDK's own
# supported switch for this, so the C side moves to match sysl.
set(PICO_HARD_FLOAT_ABI 1)

include(pico_sdk_import.cmake)

project(sysl_wifi C CXX ASM)

pico_sdk_init()

# `lwipopts.h` is the application's to supply, and the SDK's own lwIP sources include it — so it has
# to be on the include path before those targets are built, not just for this project's own code.
include_directories(${CMAKE_CURRENT_LIST_DIR})

# The sysl half, compiled to a static archive and a header by the compiler itself.
set(SYSL_ARCHIVE ${CMAKE_CURRENT_BINARY_DIR}/libwifi.a)

# **`--lib` at a sibling worktree, where `repl/` names a coordinate.** The Wi-Fi surface is not in a
# released `pico2` yet; this reverts to a `dependencies` block in `wifi/package.hocon` as soon as
# that package is tagged. Until then this project needs the worktree beside it and will not build
# from a fresh clone.
add_custom_command(
    OUTPUT ${SYSL_ARCHIVE} ${SYSL_ARCHIVE}.h
    COMMAND sysl build-c ${CMAKE_CURRENT_SOURCE_DIR}/wifi --target thumb-freestanding
            --lib $ENV{HOME}/dev/sysl-lang/pico2-wifi
            -o ${SYSL_ARCHIVE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/wifi/wifi.sysl
            ${CMAKE_CURRENT_SOURCE_DIR}/wifi/package.hocon
    COMMENT "Compiling the sysl module for thumbv8m.main"
    VERBATIM)

add_custom_target(sysl_archive DEPENDS ${SYSL_ARCHIVE})

# There is no C in this project at all. sysl exports `main` itself, so the SDK's `crt0` branches
# straight into it. CMake accepts the archive in the source list and hands it to the linker, which
# satisfies its "a target must have sources" rule without inventing a placeholder file — and with no
# C translation unit there is no language for it to infer, hence `LINKER_LANGUAGE`.
add_executable(sysl_wifi ${SYSL_ARCHIVE})

set_target_properties(sysl_wifi PROPERTIES LINKER_LANGUAGE C)

add_dependencies(sysl_wifi sysl_archive)

# **`pico_cyw43_arch_lwip_threadsafe_background`, not the `pico_cyw43_arch_none` that `repl/` uses.**
# Joining waits for the link to come up, and "up" is read out of an lwIP `netif` — with no TCP/IP
# stack linked, the radio associates and nothing can ever report it, so the join always times out.
# The *background* variant rather than `..._poll` because this program blocks inside `read_line` for
# nearly all of its life, and the polling variant would run the driver only between keystrokes.
target_link_libraries(sysl_wifi
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    ${SYSL_ARCHIVE})

pico_enable_stdio_usb(sysl_wifi 1)
pico_enable_stdio_uart(sysl_wifi 0)

pico_add_extra_outputs(sysl_wifi)
