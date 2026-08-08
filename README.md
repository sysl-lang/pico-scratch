# Pico 2 W — C SDK scratch area

Bare-metal C for a Raspberry Pi Pico 2 W (RP2350, Cortex-M33, CYW43439 wireless).

## Layout

    pico-sdk/     the Raspberry Pi C SDK — NOT in this repo, see below
    blink/        C: onboard LED + a hello-world counter over USB serial
    sysl-blink/   the same thing in sysl, with the SDK hosting it

## Getting the SDK

It is 81 MB of somebody else's repository, so it is gitignored and every checkout fetches its own.
Three submodules are needed; `btstack` deliberately is not, and the configure step will warn that
BLE is unavailable.

    git clone -b master --depth 1 https://github.com/raspberrypi/pico-sdk.git pico-sdk
    git -C pico-sdk submodule update --init --depth 1 lib/tinyusb lib/cyw43-driver lib/lwip

`pico_sdk_import.cmake` is committed in each project directory. It is a copy of the SDK's own
`external/pico_sdk_import.cmake`, which is the ordinary way to carry it, and it is what lets a
project find the SDK through `PICO_SDK_PATH`.

## Toolchain

| piece | where it comes from |
|---|---|
| `arm-none-eabi-gcc` | the **`gcc-arm-embedded` cask**, Arm's official pre-built toolchain |
| `picotool` | `brew install picotool` — the RP2350 build needs it to sign the binary |
| `cmake`, `ninja` | already present |

**Not the `arm-none-eabi-gcc` Homebrew *formula*.** It ships no newlib, so there is no `libc` or
`libg` and the link fails on the SDK's own `boot_stage2` with `cannot find -lg`. The cask puts the
same binary names in `/opt/homebrew/bin`, so the formula has to be uninstalled first or they collide.

Homebrew on this machine belongs to the **`work`** account — run any `brew` command from there.

The SDK submodules this needs are `lib/tinyusb`, `lib/cyw43-driver` and `lib/lwip`. `btstack` is
not initialised, so the configure step warns that BLE is unavailable; that is expected.

## Building

Either project, the same way — `cd blink` or `cd sysl-blink`:

    PICO_SDK_PATH=$HOME/dev/sysl-lang/pico-scratch/pico-sdk cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

The artifact to flash is `build/blink.uf2` or `build/sysl_blink.uf2`.

## sysl on the board, with the SDK hosting it

`sysl-blink/` is the same program written in sysl, and **it contains no C** — one `.sysl` file and a
`CMakeLists.txt`. The SDK still supplies the board: the linker script, the vector table, the boot
block, the clocks, and the `crt0` that calls `main`. sysl supplies `main`.

    @export("main")
    run() -> int =

`@export` publishes a definition under a plain C symbol. It is also what makes the module reachable
at all — a `sysl build-c` has no entry point of its own, so with nothing exported the whole module is
pruned away, and the compiler warns rather than leaving you to discover it. `extern` is the same idea
read the other way, and is how sysl reaches the board:

    extern "sleep_ms" sleep_ms(ms: u32)
    extern "cyw43_arch_gpio_put" cyw43_arch_gpio_put(wl_gpio: uint, on: bool)

Printing needs no extern of its own: sysl's `print` reaches `putchar`, which the SDK has wired to the
USB serial port. Only real symbols can be reached this way — much of the SDK's hardware API is
`static inline` (45 functions in `hardware/gpio.h` alone) and would need a C shim.

`CMakeLists.txt` runs the compiler itself, so `cmake --build` is the whole of it:

    sysl build-c blink --target thumb-freestanding --no-std-lib -o libblink.a

**Two flags there are load bearing**, and both are commented where they sit. `--no-std-lib` compiles
the standard module's source into the archive; without it the archive refers to library code it does
not contain and nothing can link it. And `PICO_HARD_FLOAT_ABI` is set before the SDK is imported,
because sysl's only Cortex-M33 target passes floating-point arguments in VFP registers while the SDK
defaults to `softfp` — and the linker refuses to merge the two even when no float crosses the
boundary.

Use `loop` rather than `while true` for a non-returning entry point. `loop` diverges, so `main` can
be typed `-> int` with no unreachable `return` after it.

## Flashing

The board has to be in BOOTSEL mode. Unplug it, hold the **BOOTSEL** button, plug it back in,
release — `RPI-RP2` appears under `/Volumes`. Then either:

    cp build/blink.uf2 /Volumes/RPI-RP2          # it reboots itself when the copy finishes

or, which also works once firmware built with `pico_enable_stdio_usb` is already running (picotool
can reset such a board into BOOTSEL by itself):

    picotool load -f build/blink.uf2 && picotool reboot

`picotool info -a` says what it can see.

## Watching the serial output

    screen /dev/cu.usbmodem<n> 115200

`ls /dev/cu.*` to find the number. A Pico presents Raspberry Pi's vendor id `0x2E8A`; a port with
some other id is a different device. `ioreg -p IOUSB -l -w 0 | grep -iE '"USB Product Name"|idVendor'`
prints it — `system_profiler SPUSBDataType` returns nothing under a sandbox.

**A read from the port blocks until the data arrives**, so `head -n 6 /dev/cu.usbmodem<n>` sits there
for six seconds waiting on a program that prints once a second. Redirect it to a file and run it in
the background rather than waiting on it in the foreground.

## The LED is not on a GPIO

On a Pico 2 W the onboard LED hangs off the CYW43439, not off an RP2350 pin. `pico2_w.h` says so:

    // no PICO_DEFAULT_LED_PIN - LED is on Wireless chip
    #define CYW43_WL_GPIO_LED_PIN 0

So `gpio_put` cannot reach it. A program that only blinks still has to link `pico_cyw43_arch_none`,
call `cyw43_arch_init()`, and drive the pin with `cyw43_arch_gpio_put`.
