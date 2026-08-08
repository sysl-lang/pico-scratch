// The C half of a sysl program on the Pico 2 W.
//
// Its whole job is to bring the board up — USB serial, and the wireless chip the LED hangs off —
// and then hand over. Everything after `sysl_run()` is sysl, and it does not return.

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "libblink.a.h"

int main(void) {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("cyw43_arch_init failed\n");
        return 1;
    }

    sysl_run();
}
