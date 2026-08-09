#ifndef _PICO2_MBEDTLS_CONFIG_H
#define _PICO2_MBEDTLS_CONFIG_H

// mbedtls's configuration, which is the application's file in the way `lwipopts.h` is: the library
// ships no defaults of its own that a bare-metal target could use, and every source file in it is
// compiled against whatever this says.
//
// **It starts from mbedtls's own full configuration and switches things off**, rather than naming
// each feature it wants. The other way round is smaller and is a poor trade here: a missing cipher
// suite shows up as a handshake that fails against one server and works against another, which is a
// day of debugging to save flash that a 4 MB part is not short of.

// **mbedtls 3.x made its struct fields private and lwIP's TLS layer reads them anyway.** Every
// member is declared through an `MBEDTLS_PRIVATE()` macro that mangles the name unless this is
// defined, and `altcp_tls_mbedtls.c` touches `ssl_context.out_left` and `ssl_session.start`
// directly. Without this the build fails with `'mbedtls_ssl_context' has no member named
// 'out_left'`, which reads like a version mismatch and is a deliberate access control.
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "mbedtls/mbedtls_config.h"

// ## What a board without an operating system does not have
//
// Each of these compiles its whole source file away. They are not optimisations — every one of them
// names a POSIX facility that is not here, and leaving it on is a link error at best.

// Sockets. lwIP is the network stack; mbedtls is only ever handed bytes by `altcp_tls`.
#undef MBEDTLS_NET_C

// `gettimeofday` and friends, used for timeouts mbedtls does not need under altcp.
#undef MBEDTLS_TIMING_C

// A filesystem, for reading keys and certificates off disk. Ours are compiled in.
#undef MBEDTLS_FS_IO
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C

// ## The clock, which is the one real compromise here
//
// **A certificate carries a validity range, and checking it needs to know what day it is.** This
// board does not: with no battery-backed clock and no SNTP, `time()` counts from boot, so every
// certificate on the internet reads as not yet valid and every handshake fails.
//
// Switching the check off is what makes a fetch work at all, and it is worth being plain about what
// it costs: **an expired or revoked certificate is accepted**. The signature chain is still verified
// in full, so this is not the same as trusting anything — a forged certificate is still rejected —
// but a genuine one that has since expired is not caught.
//
// The fix is SNTP, which lwIP has (`pico_lwip_sntp`) and which is a separate piece of work: get the
// time, then turn this back on.
//
// **Only the date check goes; `MBEDTLS_HAVE_TIME` stays.** They look like a pair and are not. The
// second controls whether a `mbedtls_ssl_session` has a `start` field at all, and
// `altcp_tls_set_session` reads it — so removing it takes lwIP's TLS layer out with it, which is a
// compile error two libraries away from anything this file appears to be about.
#undef MBEDTLS_HAVE_TIME_DATE

// **And keeping `MBEDTLS_HAVE_TIME` means owing mbedtls a millisecond clock.** It implements
// `mbedtls_ms_time` over `clock_gettime` on Linux and `GetSystemTimeAsFileTime` on Windows, and
// `#error`s on anything else — so a bare-metal target must say it will supply its own. `http.c` does,
// from `pico/time.h`, which is three lines and is the only part of this that is really the board's.
#define MBEDTLS_PLATFORM_MS_TIME_ALT

// ## Randomness
//
// **`/dev/urandom` is what mbedtls reaches for by default and there is none.** The SDK supplies
// `mbedtls_hardware_poll` from the RP2350's true random number generator — `pico_mbedtls.c` — so
// pointing mbedtls at that and forbidding the platform source is the whole of it.
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_NO_PLATFORM_ENTROPY

// ## Two things deliberately left as mbedtls has them
//
// **The RP2350's SHA-256 accelerator is NOT enabled**, though `pico_mbedtls` offers it as
// `MBEDTLS_SHA256_ALT`. Under `..._lwip_threadsafe_background` every mbedtls call runs inside an
// interrupt, and the accelerator is a single shared peripheral taken with a blocking wait — so a
// handshake in the interrupt would block on hardware the main thread might be holding. Software
// SHA-256 on a 150 MHz Cortex-M33 is fast enough for a handshake, and this is a deadlock class
// avoided rather than a speed-up declined lightly.
//
// **TLS 1.3 is left on.** It needs `psa_crypto_init()` to have been called, which lwIP's TLS layer
// never does — `http.c` does it once at start-up, and says so there.

// The record buffers, one pair per connection off the C heap. **Incoming stays at mbedtls's 16 KB**
// because that is the largest record a server may send and nothing here negotiates a smaller one;
// outgoing is ours alone and a GET request never approaches 4 KB.
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096

// The tests mbedtls can run against itself, which nothing here calls.
#undef MBEDTLS_SELF_TEST

#endif
