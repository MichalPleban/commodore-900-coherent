# The Commodore 900 boot ROM

The machine's 32 KB firmware: power-on diagnostics, the resident `ddt`
monitor, and the bootstrap that reads a Coherent `l.out` off the hard disk
(or a floppy) and jumps to it.  This is the code that prints

    Commodore C900 diagnostics V1.0 (6/14/85)
    Memory test: 1024K OK
    ...
    Automatic boot in progress

before the kernel banner appears.

The sources are Kevin Dedon's decompilation of the shipping V1.0 ROM
(`github.com/kdedon/commodore-900-bios`), ported here so they build with the
same cross toolchain as the rest of the tree.  Upstream builds with the
1985 MWC `cc`/`as`/`nld` **running on the C900 itself** and reproduces the
shipping EPROMs byte for byte; this port builds with PCC and therefore does
not — see *Fidelity* below.

## Building

    make bios

Outputs land in `build/rom/`:

| file | what |
|---|---|
| `rom.bin` | the flat 32 KB EPROM image |
| `bios_h.bin` | even (high) bytes — one EPROM |
| `bios_l.bin` | odd (low) bytes — the other |

`build/obj/bios/rom` is the unstripped `l.out` the image is made from; feed
it to `tools/segmap.py` to see where the bytes went.

Run it in either emulator with `--firmware=build/rom` (they load a directory
by the `bios_h.bin`/`bios_l.bin` names; upstream and EPROM programmers call
the same two files `rom.H`/`rom.L`):

    c900.exe --disk=<copy of hdd.bin> --firmware=build/rom --max=120000000 \
             --input="root\r"

`../Emulator/rom/` still holds the original 1985 ROM; nothing overwrites it,
so the two are directly comparable.

## How the ROM is laid out

The link is `ld -i -R 0` — separate I&D, text based at 0, because the ROM is
mapped at physical address 0 at reset.  `tools/mkrom.py` then flattens the
`l.out` into the image:

    0x0000 - 0x67ff   text (26624 bytes max)
    0x6800 - 0x7fff   initialised data (6144 bytes max)

There is no bss in the image.  At reset `lkcrt.s` (`sizeram_`) programs MMU
descriptor **0x3f** to a window at the **top of physical RAM**, copies the
data half of the image there, zeroes bss behind it, and sets the stack to
the top of that window.  Every C global the ROM has therefore lives in
hardware segment 0x3f, and `SS = 0x3F3F` in the assembly sources is the
segment prefix that reaches it.  The window is `RAMWINK` KB (lkcrt.s);
`mkrom.py --window` is told the same number and fails the build if
data + bss + a 512-byte stack reserve does not fit.

## What this port changed

Everything else is upstream's, byte for byte.

1. **`lkcrt.s` — the RAM window and stack are parameters, not constants.**
   The 1985 ROM hard-codes a 7 KB window (`0x1c45 >> 10`), a data-copy length
   of `0x1016` and a stack top of `0x121c`: exactly its own data size plus
   518 bytes of stack.  This build's data is bigger (4644 + 1564 of bss
   against 4118), so those three constants are now `RAMWINK`/`STKTOP` and
   the linker's `edata_`/`end_`.  The window is 12 KB, which leaves ~6 KB of
   stack instead of the original's 518 bytes.  The extra 5 KB costs nothing:
   it is at the top of RAM, the kernel loads at the bottom, and the window is
   dead the moment `fsload` jumps to the kernel.
2. **`lkcrt.s` — bss is zeroed.**  The original copies initialised data and
   nothing else; whatever the memory test left in bss (0xaaaa) stayed there.
   The copy is now followed by a clear of `edata_ .. end_`, so C globals
   start at zero as the language promises.
3. **`boot.c` — three casts.**  `dp = &buf[0]` / `dp < &buf[512]` (a
   `struct direct *` walked over a `char[512]`) and `lp = buf`, so the build
   stays warning-free.  No generated code changes.
4. **`ddt.s` — two key tables addressed by label, not by their 1985 address.**
   `dispatch` and `bpswitch` find a command by `cpir`-scanning a table of
   command letters that sits immediately after them, and the decompilation
   kept the address of each table as the literal it was in the shipping
   image (`$0x000039ba` and `$0x000031c4`, each also subtracted back off to
   get the index).  Relinked anywhere else those point into unrelated code,
   the scan never matches, and **every debugger command answers `Illegal
   command`** — which is exactly what this port did until the tables were
   given labels (`cmdkeys`, `bpkeys`).  The instructions assemble to the same
   forms, now relocatable.  This is a latent bug in the upstream sources, not
   something the port introduced: it cannot show there, because upstream
   reproduces the original layout exactly.  The rest of the ROM assembly was
   swept for the same idiom and these two were the only absolute text
   addresses; the `symbol+offset` reaches that deliberately run past their own
   array (`ddtmsgs_+0xa3` into `m_err`, `ddtbss_+0x104`) were checked against
   the linked image and land where they should.

`bootfix.c` and `prom.c` are upstream's on-machine flatten/split tools, kept
for a native rebuild on the C900.  The host build does both in
`tools/mkrom.py` instead and does not compile them.

Two upstream build steps were also dropped as no-ops: every `.s` except
`lkcrt.s` went through `cpp` without containing a single macro or
conditional, and `cpp` reads the assembler's `.word 'M` character literals
as unterminated ones — so only `lkcrt.s` is preprocessed now.

## Fidelity

Upstream's first goal is a byte-exact reproduction of the shipping ROM, and
that requires the original compiler; **do not expect it here**.  PCC lays out
code differently, so this image is a different ROM that does the same thing.
For reference, at the time of the port:

|  | 1985 (MWC) | this build (PCC `-O`) |
|---|---|---|
| text | 26310 | 25606 |
| data | 4118 | 4644 |
| bss | (in data) | 1564 |

`-O` is not optional: without it the text is 28116 bytes and overflows the
ROM's 26624-byte text area by ~1.5 KB.

## Verified

Both emulators, booting `build/dist/hdd.bin` off the hard disk with
`--firmware=build/rom`:

* serial machine (`../Emulator/bin/c900.exe`) — diagnostics, autoboot,
  kernel banner, `Coherent login:`, a logged-in shell running commands;
* hi-res machine (`../JS/engtest.exe --video=hires`) — same, up to the
  `/etc/zlogin` graphical panel, which also exercises the ROM's video-board
  detection and its alternate-console output path;
* **floppy** — with `build/dist/floppy.img` attached the autoboot's first
  try, `(fd,1)coherent`, loads the `fdcon` kernel off the floppy and boots
  it.  Proven against a **blank** hard disk, so nothing came from the HD;
* an unbootable disk — `bad file type`, `cannot boot!` and the monitor's
  "Insert bootable floppy" prompt, i.e. the error path and command loop;
* **the monitor and `ddt`** — typing `1` `9` during the serial probe sets
  the abort flag and skips autoboot straight to the `? ` prompt.  `?` lists
  the commands, `m` reports the sized RAM (`08|0000` to `18|0000`), `d`
  enters the debugger, and there `?`, `r` (registers), `M` (MMU descriptors)
  and `h 1234 5678` (hex sum/difference) all answer correctly.

Every one of those was run against the original 1985 EPROMs too, with the
same script, and the output matches — the sole intended difference being the
RAM window in the `M` dump: descriptors 01 and 3F read `17D0` here (ramtop −
12K) against the original's `17E4` (ramtop − 7K).

Driving the ROM's own prompts from a script needs an emulator that will feed
`--input` before a Coherent prompt appears: stock `c900.exe` starts feeding
only once it sees `#` or `login: ` in the output, and the ROM's `? ` and `*`
prompts mean nothing to it.

## Licence

The sources carry a Kevin Dedon copyright and this tree's BSD-3-Clause
header, matching the licence of the ROM they were recovered from
(`../Emulator/rom/LICENSE.txt`).  Upstream ships no `LICENSE` file; if it
gains one that says otherwise, these headers should follow it.
