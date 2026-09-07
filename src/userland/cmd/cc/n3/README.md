# `n3` — cc3, the intermediate-language printer

cc3 reads an intermediate file and prints it. It understands both flavours:
the **trees** cc0 writes (`.0`) and the **CODE records** cc1 writes (`.1`). It
emits no code and nothing in the pipeline depends on it.

	cc3-z8001 <variant> <input> [output]

	cc0-z8001 800000000800 x.c x.z0
	cc3-z8001 800000000800 x.z0        # the trees cc1 will be given
	cc1-z8001 800000000800 x.z0 x.z1
	cc3-z8001 800000000800 x.z1        # the instructions cc2 will encode

`cc -3` reaches it through the driver's existing pass table (`coh/cc.c`).

Machine-independent files come from donor `v4.2.12/c/n3`; the machine layer is
`z8001/`, templated on the donor **i8086** (16-bit segmented), with the name
tables in `common/z8001/`. `z8001/icode.c` carries its own copy of
`n2/z8001/optab.c`'s rows — `tests/cc3tab.sh` (`make check-cc3tab`) is what
keeps the copy honest, and it is not optional: the i1 stream is self-delimiting
only if the reader knows each opcode's operand count.

## Checked against the original

The 1985 Z8001 distribution shipped an 18,282-byte `cc3`
(`commodore-900-coherent/src/dist/lib/cc3`), and it runs under
`commodore-900-emulator`'s `c900 --exec`. Feeding both it and this cc3 the same
intermediate file is the strongest faithfulness check available, and on a
tree-free stream (globals only) the two dumps agree line for line, offset for
offset, including record names and label emission.

Three things had to be understood to get there, and they are worth recording.

**The original binaries need `LF_SEP` set to load.** All five 1985 passes
(`cc0`…`cc3`, `cpp`) are marked `LF_SHR|LF_NRB|LF_32` — shared text, no
relocation bits, 32-bit format — and *not* `LF_SEP`. But their crt0 stores
`environ` into segment 4 while their text is in segment 3, so they are
separated-I/D binaries in everything but the flag. `c900 --exec` decides
separation on `LF_SEP` alone (`src/uexec.c`), loads their data on top of the
text segment, and libc's stdio table is then garbage: `fopen` returns NULL
without ever issuing `open(2)`, the "cannot open" message goes to a stderr that
does not work, and the pass exits 1 having made exactly one syscall. Setting
`LF_SEP` on a scratch copy makes all of it work. `cpp` gets further than the
others only because it uses raw syscalls rather than stdio. The rule the
emulator is missing is presumably that `LF_SHR` implies separated I/D — shared
text has to be separate from data by definition.

**Our IR is not the original's IR**, so the comparison can only ever cover the
machine-independent framing:

| | original 1985 | this compiler |
|---|---|---|
| `sizeof_t` (`zget`) | 16-bit | 32-bit (`SIZEOF_LARGE`) |
| type codes | `S8 U8 S16 U16 S32 U32 PTR F32 F64 PTB BLK FLD8 FLD16 FLD32` | the i8086 set: `… F32 F64 BLK FLD8 FLD16 LPTR LPTB SPTR SPTB` |
| byte-register numbers | `rl0..rl7` then `rh0..rh7` | `RH0..RH7` then `RL0..RL7` |
| opcode indices | its own order | `generated/opcode.h` order |

The type-code difference is the fatal one for a whole-file diff: `BLK` is 10
there and 8 here, and `itree` reads an extra word for a `BLK` node, so the
original desynchronizes on the first block-typed tree in one of our files. On a
stream with no expression trees the only visible differences are the `sizeof_t`
width (the original consumes 2 bytes where we wrote 4) and byte order (our
*host* build writes little-endian; the target reads big-endian) — both
artefacts of handing a host-built compiler's output to a target-native reader,
neither a disagreement about the format.

**The original's vocabulary is otherwise ours.** Its string table holds the
same `ilonames`, `dnames`, segment names, `.byte/.word/.long` directives,
`genname`/`genival` formats, and the same operand syntaxes (`@rr0`, `rr0(disp)`,
`rr0(r1)`, `addr(r1)`); its condition-code names — including `un` for the
unconditional branch, which is also what `as-z8001` accepts — are used here.
Its segment directives (`.shri .link .shrd .strn .prvd .bssd`) are the ones
`z8001/igen.c` prints.
