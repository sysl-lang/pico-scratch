// Onboard LED blink for the Pico 2 W, with a heartbeat over USB serial.
//
// On a Pico 2 W the LED is not on an RP2350 GPIO — it hangs off the CYW43439
// wireless chip, so it is driven through cyw43_arch rather than gpio_put.

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

int main(void) {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("cyw43_arch_init failed\n");
        return 1;
    }

    for (unsigned i = 0;; i++) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
        sleep_ms(250);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
        sleep_ms(750);
        printf("blink %u\n", i);
    }
}
