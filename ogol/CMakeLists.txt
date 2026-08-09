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
add_custom_command(
    OUTPUT ${SYSL_ARCHIVE} ${SYSL_ARCHIVE}.h
    COMMAND sysl build-c ${CMAKE_CURRENT_SOURCE_DIR}/ogol --target thumb-freestanding-softfp
            --lib $ENV{HOME}/dev/sysl-lang/ogol
            -o ${SYSL_ARCHIVE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ogol/main.sysl
            ${CMAKE_CURRENT_SOURCE_DIR}/ogol/package.hocon
    COMMENT "Compiling Ogol and its console for thumbv8m.main"
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
