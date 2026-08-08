cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# sysl's only Cortex-M33 target is `thumbv8m.main-none-eabihf`, which passes floating-point
# arguments in VFP registers. The SDK defaults to `softfp`, which passes them in integer registers,
# and GNU ld refuses to merge objects that disagree — "uses VFP register arguments, <output> does
# not" — whether or not any float ever crosses the boundary. `PICO_HARD_FLOAT_ABI` is the SDK's own
# supported switch for this, so the C side moves to match sysl.
set(PICO_HARD_FLOAT_ABI 1)

include(pico_sdk_import.cmake)

project(sysl_blink C CXX ASM)

pico_sdk_init()

# The sysl half, compiled to a static archive and a header by the compiler itself. `build-c` writes
# both, and names the header after the archive.
set(SYSL_ARCHIVE ${CMAKE_CURRENT_BINARY_DIR}/libblink.a)

add_custom_command(
    OUTPUT ${SYSL_ARCHIVE} ${SYSL_ARCHIVE}.h
    # `--no-std-lib` compiles the standard module's source into the archive. Without it the archive
    # is left referring to `sysl$prints` and friends, which nothing else provides, so a C project
    # cannot link it.
    COMMAND sysl build-c ${CMAKE_CURRENT_SOURCE_DIR}/blink --target thumb-freestanding --no-std-lib
            -o ${SYSL_ARCHIVE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/blink/blink.sysl
    COMMENT "Compiling the sysl module for thumbv8m.main"
    VERBATIM)

add_custom_target(sysl_archive DEPENDS ${SYSL_ARCHIVE})

add_executable(sysl_blink main.c)

add_dependencies(sysl_blink sysl_archive)

# The generated header lands beside the archive in the build directory.
target_include_directories(sysl_blink PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(sysl_blink pico_stdlib pico_cyw43_arch_none ${SYSL_ARCHIVE})

pico_enable_stdio_usb(sysl_blink 1)
pico_enable_stdio_uart(sysl_blink 0)

pico_add_extra_outputs(sysl_blink)
