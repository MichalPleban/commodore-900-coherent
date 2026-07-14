# compress — port to Coherent 0.7.3 (Z8001 / PCC)

`compress`/`uncompress`/`zcat` — the LZW file compressor (IEEE Computer, June
1984). The sources are the Coherent 3.2 tree (`compress.c` + `is_fs.c`, the
Charles Fiterman COHERENT port with the small-model **VIRTUAL** scheme); the
untouched import is in `_port/compress/`. Built with the modern Z8001 PCC
cross-toolchain and `-DVIRTUAL -DBITS=16`, exactly as the 3.2 makefile did.

## Why VIRTUAL / BITS=16

At 16-bit codes the hash and code tables are far larger than one Z8001 64K data
segment (`htab` alone is `HSIZE*4` ≈ 276K). The VIRTUAL scheme keeps a small
32K in-core cache (`data[512][64]`) and pages the full tables through a scratch
file. `maxbits<=12` stays entirely in that flat buffer (no scratch file); codes
wider than 12 bits — which includes **decompressing any standard `.Z`** — page
through the scratch file. BITS=16 is required so `uncompress` can read ordinary
16-bit `.Z` files.

## Changes from the 3.2 source (all in `compress.c`)

Only the host interface changed; the codec is untouched.

1. **`strrchr` → `rindex`.** This libc predates the ANSI `<string.h>` names;
   `#define strrchr rindex` maps to the V7/BSD name libc actually ships.
2. **`memset` provided locally.** 0.7.3 libc has no `memset`; a small definition
   is added (only the VIRTUAL block needs it, to zero a freshly-paged block).
3. **Scratch store: `/dev/ram1` → a `/tmp` file.** The 3.2 code defaulted to a
   `/dev/ram1` ram-disk (with an `is_fs()` safety probe and a `/dev/ram1close`
   reset device). This kernel has no working `/dev/ram`, so `initV()` now uses a
   `mktemp("/tmp/czXXXXXX")` scratch file, created and immediately unlinked so it
   vanishes on exit. `-w` still names an alternative; if `-w` names a device
   special file, the original `is_fs()` check still runs and the device is used
   raw (`is_fs.c` is unchanged). `closeRam()` dropped the `/dev/ram1close` reset.

`/tmp` is a separate filesystem (`/dev/hd3`) mounted by `/etc/rc`.

## Build / install

Wired into the top-level `Makefile` (`$(USRBINDIR)/compress`): one binary in
`/usr/bin`, with `/usr/bin/uncompress` and `/usr/bin/zcat` as hardlinks
(behaviour keys off `argv[0]`) declared in `src/image/hdd_manifest.txt`.

## Verified in the emulator

Round-trips reproduce the input byte-for-byte (`cmp` clean, exit 0) for:
the default 12-bit in-memory path; the 16-bit `-b 16` path (exercising the
`/tmp` disk-paging); and decompression via both the `uncompress` and `zcat`
hardlink names. A 5642-byte text file compressed to 1433 bytes.
