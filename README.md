# Pico 2 W — C SDK scratch area

Bare-metal C **and sysl** for a Raspberry Pi Pico 2 W (RP2350, Cortex-M33, CYW43439 wireless).

It is a testbed rather than a component — the name says so on purpose. What it is for is finding out
what sysl on real silicon actually needs, and it has been good at that: seven compiler and library
tickets came out of writing the sysl programs here, and all of them shipped.

## Layout

    pico-sdk/     the Raspberry Pi C SDK — NOT in this repo, see below
    blink/        C: onboard LED + a hello-world counter over USB serial
    sysl-blink/   the same thing in sysl, with the SDK hosting it
    repl/         a REPL in sysl over the USB serial port, with line editing
    wifi/         the radio, driven from that same REPL — scan, join, resolve, fetch

**`blink/` is kept deliberately.** It is the control: when something stops working, the question is
always whether it is the board, the toolchain or sysl, and a C program that has never changed answers
the first two in one build.

No sysl project here contains a line of C. The SDK supplies the board — the linker script, the vector
table, the boot block, the clocks, the `crt0` that calls `main` — and
[`sh.sysl.pico2`](https://github.com/sysl-lang/pico2) declares the entry points.

**The line editor is the standard library's**, which it was not always. `repl/` and `wifi/` build a
`sysl.term.edit.Editor` over the `Reader` and `Writer` `pico2` supplies for the USB port, so the
console here is the same code a program at a desktop terminal runs — a board and a laptop differ in
where the bytes come from and in nothing else. Until pico2 v0.0.7 that editor was `pico2.read_line`,
two hundred lines living in the board's package because nothing shipped with the language would echo
a keystroke.

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

Any of them, the same way — `cd blink`, `cd sysl-blink`, `cd repl` or `cd wifi`:

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

    sysl build-c blink --target thumb-freestanding -o libblink.a

**The float ABI is what makes the target name end in `-softfp`**, and it is commented where it sits.
GNU ld refuses to merge objects whose float ABIs disagree — *"uses VFP register arguments, \<output\>
does not"* — whether or not a float ever crosses the boundary, so one side has to move. Until sysl
0.0.35 the only Cortex-M33 target was `thumbv8m.main-none-eabihf` and these projects set the SDK's
`PICO_HARD_FLOAT_ABI` so the C side matched; sysl gained a softfp sibling in that release, so this is
a stock pico-sdk now and it is sysl that follows.

That command needed a `--no-std-lib` until **sysl 0.0.33**, because a `build-c` archive was left
referring to library code it did not contain. That is what kept this repository private: its headline
lesson would have been a compiler defect. The archive is self-contained now.

Use `loop` rather than `while true` for a non-returning entry point. `loop` diverges, so `main` can
be typed `-> int` with no unreachable `return` after it.

**A dependency's C is compiled whole, whatever the program imports.** `pico2` carries four C files
for the radio, so a project naming a tag from v0.0.6 on has to hand `sysl build-c` the SDK's include
directories and compile definitions before `dns.c` gets past `pico/cyw43_arch.h` — which is why
`repl/` has an include block and `sysl-blink/`, pinned at v0.0.4, does not. It costs the *compile*
and not the link: the four objects go into the archive unreferenced and stay there, so `repl.elf` is
319 KB with no lwIP in it against `wifi.elf`'s 788 KB.

## Flashing

The board has to be in BOOTSEL mode. Unplug it, hold the **BOOTSEL** button, plug it back in,
release — **`RP2350`** appears under `/Volumes`. (`RPI-RP2` is the original Pico's name; looking for
that one here finds nothing.) Then either:

    cp build/blink.uf2 /Volumes/RP2350           # it reboots itself when the copy finishes

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
