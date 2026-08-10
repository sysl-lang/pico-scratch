cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# No `PICO_HARD_FLOAT_ABI`: `thumb-freestanding-softfp` is what lets this be a stock pico-sdk, with
# sysl following the SDK's default rather than the SDK following sysl. See `repl/CMakeLists.txt`,
# which carries the whole of that story.

include(pico_sdk_import.cmake)

project(sysl_ogol C CXX ASM)

pico_sdk_init()

set(SYSL_ARCHIVE ${CMAKE_CURRENT_BINARY_DIR}/libogol.a)

# **The language is here by `--lib` because `sysl-lang/ogol` is not tagged yet**, which is the same
# arrangement `wifi/` has and costs the same thing: this project does not build from a fresh clone.
# The board is an ordinary coordinate in `ogol/package.hocon`.
#
# It was a second `--lib` until sysl 0.0.37, and not by choice — card `0055`. A `--lib` source root
# binding a top-level name silently shadowed the sub-modules of a **coordinate** dependency binding
# the same one, and every package in this org binds `sh`, so `--lib ogol` beside
# `pico2 { git = … }` reported `no module is called 'sh.sysl.pico2.externs'` from inside the cached
# package. **This needs sysl 0.0.37 or newer**; an older one wants the coordinate turned back into a
# second `--lib`.
# Which checkout of the language to build against. It is a cache variable rather than a fixed path so
# that a branch can be tried on the board without editing this file — `-DOGOL_LIB=…/ogol-lists` — and
# so that what was built is recorded in CMakeCache.txt rather than remembered.
set(OGOL_LIB "$ENV{HOME}/dev/sysl-lang/ogol" CACHE PATH "The sysl-lang/ogol checkout to build against")

# **`pico2` carries C now, and it needs the SDK's headers to compile.**
#
# This project needed none of the following while it named pico2 v0.0.4, because that version was
# pure declaration — every name in it was a symbol the SDK already had. v0.0.5 onwards carries
# `scan.c` and `dns.c` for the radio, and `sysl build-c` is a program CMake knows nothing about, so
# what it can see has to be handed to it. `wifi/CMakeLists.txt` carries the whole of this story.
#
# The libc half comes from the toolchain rather than being written down: `-print-sysroot` answers with
# the directory whose `include` holds newlib's headers, and the obvious path one level up has no
# `string.h` in it.
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-sysroot
    OUTPUT_VARIABLE ARM_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

# The SDK half is asked of the SDK: it generates some of its headers into its own build directory, so
# a fixed list written here would go stale the first time it moved one. What is asked is the
# **executable's** resolved list rather than any one library's, because `cyw43.h` includes
# `lwip/netif.h`, which is two link libraries away — reading `INCLUDE_DIRECTORIES` off the target gets
# the transitive closure CMake has already worked out for the C side. It names a target defined
# further down, which is fine: a generator expression is resolved after the whole file is read.
set(SYSL_C_INCLUDES
    ${ARM_SYSROOT}/include
    $<TARGET_PROPERTY:sysl_ogol,INCLUDE_DIRECTORIES>)

# And the macros, which finding the headers does not supply: `pico/cyw43_arch.h` `#error`s rather than
# picking an architecture variant, so every include path can be right and the compile still stops
# inside a header.
set(SYSL_C_DEFINES $<TARGET_PROPERTY:sysl_ogol,COMPILE_DEFINITIONS>)

add_custom_command(
    OUTPUT ${SYSL_ARCHIVE} ${SYSL_ARCHIVE}.h
    COMMAND sysl build-c ${CMAKE_CURRENT_SOURCE_DIR}/ogol --target thumb-freestanding-softfp
            --lib ${OGOL_LIB}
            "$<$<BOOL:${SYSL_C_INCLUDES}>:--include-path;$<JOIN:${SYSL_C_INCLUDES},;--include-path;>>"
            "$<$<BOOL:${SYSL_C_DEFINES}>:-D;$<JOIN:${SYSL_C_DEFINES},;-D;>>"
            -o ${SYSL_ARCHIVE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ogol/main.sysl
            ${CMAKE_CURRENT_SOURCE_DIR}/ogol/package.hocon
    COMMENT "Compiling Ogol and its console for thumbv8m.main"
    # **`COMMAND_EXPAND_LISTS`, without which the two generator expressions above are useless.** A
    # quoted argument that expands to a `;`-separated list stays *one* argument unless this is given,
    # so `sysl` is handed a single `--include-path;/a;--include-path;/b` and answers
    # `Unknown option`. Unquoting is not the fix either — CMake then splits the variable at parse
    # time and the `$<` never evaluates at all.
    COMMAND_EXPAND_LISTS
    VERBATIM)

add_custom_target(sysl_archive DEPENDS ${SYSL_ARCHIVE})

# There is no C in this project at all: sysl exports `main`, the SDK's `crt0` branches straight into
# it, and the archive in the source list is what satisfies CMake's "a target must have sources" rule
# with no translation unit to infer a language from.
add_executable(sysl_ogol ${SYSL_ARCHIVE})

set_target_properties(sysl_ogol PROPERTIES LINKER_LANGUAGE C)

add_dependencies(sysl_ogol sysl_archive)

target_link_libraries(sysl_ogol pico_stdlib pico_cyw43_arch_none ${SYSL_ARCHIVE})

pico_enable_stdio_usb(sysl_ogol 1)
pico_enable_stdio_uart(sysl_ogol 0)

pico_add_extra_outputs(sysl_ogol)
