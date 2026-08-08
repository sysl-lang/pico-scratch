// The C half of a sysl program on the Pico 2 W, and there is deliberately almost nothing in it.
//
// The SDK supplies the board — the linker script, the vector table, the boot block, the clocks, and
// the `crt0` that calls `main`. Bringing the peripherals up needs no C, because the SDK's entry
// points for it are ordinary symbols that sysl can name with `extern`; so that lives in `blink.sysl`
// beside the rest of the program.
//
// What is left here is the one thing C must still own: `main` itself, which is what `crt0` branches
// to and what CMake needs a source file for.

#include "libblink.a.h"

int main(void) {
    sysl_run();
}
