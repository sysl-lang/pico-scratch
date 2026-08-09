cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# **No `PICO_HARD_FLOAT_ABI` here, and that is the point of `thumb-freestanding-softfp`.** GNU ld
# refuses to merge objects whose float ABIs disagree — "uses VFP register arguments, <output> does
# not" — whether or not a float ever crosses the boundary, so somebody has to move. Until sysl
# 0.0.35 its only Cortex-M33 target was `thumbv8m.main-none-eabihf`, and this project set the SDK's
# switch so the C side matched sysl. That was backwards for a language whose `@export` claim is that
# it joins somebody else's build: the SDK's default is `softfp`, so this is a stock pico-sdk now and
# it is sysl that follows.

include(pico_sdk_import.cmake)

project(sysl_wifi C CXX ASM)

pico_sdk_init()

# `lwipopts.h` is the application's to supply, and the SDK's own lwIP sources include it — so it has
# to be on the include path before those targets are built, not just for this project's own code.
include_directories(${CMAKE_CURRENT_LIST_DIR})

# The sysl half, compiled to a static archive and a header by the compiler itself.
set(SYSL_ARCHIVE ${CMAKE_CURRENT_BINARY_DIR}/libwifi.a)

# **The C in the `--lib` tree needs headers, and `include_directories()` above does not reach it.**
# That directive is for CMake's own C targets; the `add_custom_command` below runs `sysl build-c`,
# which is a program CMake knows nothing about, so what it can see has to be handed to it. `pico2`'s
# `scan.c` includes `<string.h>`, `pico/cyw43_arch.h` and `cyw43.h`, and it saw none of them.
#
# The libc half comes from the toolchain rather than being written down: `-print-sysroot` answers
# with the directory whose `include` holds newlib's headers, which is worth asking for rather than
# spelling, since the obvious path one level up has no `string.h` in it and fails in exactly the way
# passing nothing does.
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-sysroot
    OUTPUT_VARIABLE ARM_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

# The SDK half is asked of the SDK, for the same reason: the pico-sdk generates some of its headers
# into its own build directory, so the list is not a fixed set of source paths and a copy written out
# here would go stale the first time the SDK moved one.
#
# **What is asked is the executable's own resolved list, not any one library's.** `cyw43.h` includes
# `lwip/netif.h`, which is two link libraries away from where `scan.c` sits, so naming the obvious
# targets one at a time gets the headers of each and none of what they in turn pull in. Reading
# `INCLUDE_DIRECTORIES` off `sysl_wifi` gets the transitive closure CMake has already worked out for
# the C side, which is by construction the set the sysl side needs too -- and it stays right when the
# link libraries below change. It names a target defined further down, which is fine: a generator
# expression is resolved after the whole file has been read.
set(SYSL_C_INCLUDES
    ${ARM_SYSROOT}/include
    ${CMAKE_CURRENT_LIST_DIR}
    $<TARGET_PROPERTY:sysl_wifi,INCLUDE_DIRECTORIES>)

# **And the macros, which finding the headers does not supply.** `pico/cyw43_arch.h` will not compile
# for a build that has not said which architecture variant it means: it `#error`s on the spot rather
# than picking one, so every include path can be right and the compile still stops inside a header.
# The list is read off `sysl_wifi` for the same reason the paths are -- it is what CMake decided to
# build the C side with, and the sysl side is joining that build rather than starting its own.
set(SYSL_C_DEFINES $<TARGET_PROPERTY:sysl_wifi,COMPILE_DEFINITIONS>)

# Every source the archive is actually made of, so that editing any of them rebuilds it.
#
# **This used to name `wifi/wifi.sysl` and nothing else**, which cost an hour: neither editing the
# `--lib` tree's C nor upgrading the compiler re-ran `sysl build-c`, so a `libwifi.a` built the night
# before by an older compiler was reused and reported the *old* failure. The fix looked like it had
# not worked when it had never been run, and `ar t` on the archive is what settled it.
file(GLOB_RECURSE SYSL_LIB_SOURCES CONFIGURE_DEPENDS
    $ENV{HOME}/dev/sysl-lang/pico2-wifi/*.sysl
    $ENV{HOME}/dev/sysl-lang/pico2-wifi/*.c
    $ENV{HOME}/dev/sysl-lang/pico2-wifi/*.h
    $ENV{HOME}/dev/sysl-lang/pico2-wifi/*.hocon)

find_program(SYSL_COMPILER sysl REQUIRED)

# **`--lib` at a sibling worktree, where `repl/` names a coordinate.** The Wi-Fi surface is not in a
# released `pico2` yet; this reverts to a `dependencies` block in `wifi/package.hocon` as soon as
# that package is tagged. Until then this project needs the worktree beside it and will not build
# from a fresh clone.
add_custom_command(
    OUTPUT ${SYSL_ARCHIVE} ${SYSL_ARCHIVE}.h
    COMMAND ${SYSL_COMPILER} build-c ${CMAKE_CURRENT_SOURCE_DIR}/wifi --target thumb-freestanding-softfp
            --lib $ENV{HOME}/dev/sysl-lang/pico2-wifi
            "$<$<BOOL:${SYSL_C_INCLUDES}>:--include-path;$<JOIN:${SYSL_C_INCLUDES},;--include-path;>>"
            "$<$<BOOL:${SYSL_C_DEFINES}>:-D;$<JOIN:${SYSL_C_DEFINES},;-D;>>"
            -o ${SYSL_ARCHIVE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/wifi/wifi.sysl
            ${CMAKE_CURRENT_SOURCE_DIR}/wifi/package.hocon
            ${SYSL_LIB_SOURCES}
            ${SYSL_COMPILER}
    COMMENT "Compiling the sysl module for thumbv8m.main"
    COMMAND_EXPAND_LISTS
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
