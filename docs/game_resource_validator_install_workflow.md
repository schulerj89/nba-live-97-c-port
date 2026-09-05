# GAMEONLY resource-validator installer

`src/recovered/game_resource_validator_install.c` owns GAMEONLY
`0x800A3E20..0x800A3E37`, the six-instruction routine called at main PC
`0x80029ABC` immediately after `SetDispMask(1)`.

## What it does

The routine forms `0x800A3D60` in `v0`, stores it to callback global
`0x800D7B1C`, and returns. It has no stack frame, arguments, branches, reads,
or child calls. Its one source-visible write is at `0x800A3E2C`.

Read-only Ghidra evidence records six instructions and SHA-256
`dcc8c3b23bcde437604cbec4acb2fd0254090158205461b51adf11c77381e137`.
The callback global has one consumer: GAMEONLY loader `0x8009425C`, which
loads the pointer at `0x80094300` and calls it when nonzero.

The installed 48-instruction target is structurally equivalent to FEONLY
`0x8008ABF0`, whose installer is `0x8008ACB0`. It recognizes the `CRCF`
whole-file trailer, checks the stored checksum, optionally shrinks an accepted
allocation, and releases a rejected allocation. This change deliberately does
not claim target `0x800A3D60`; it remains a separate untranslated function.
The following main instructions configure the GAMEONLY target through globals
`0x800D7AF4` and `0x800D7AF8`.

## Preserved source behavior

- The old callback word is overwritten without being read or returned.
- No null, alignment, or ownership check is applied to the constant target.
- The installer does not invoke the callback or validate a file.
- `v0` retains `0x800A3D60`, an incidental pointer return ignored by main.
- A native refusal retains the already formed `v0`; it does not invent a
  rollback or restore the previous callback.

The installed validator's known ignored-trailer-length quirk is not repaired
here. It belongs to the separate callback body and remains documented by the
existing frontend resource validation work.

## Native and visual composition

The GAMEONLY main composition now executes this owner at the address-bearing
call boundary and writes the callback into retained RAM. It does not redirect
the native host filesystem loader: native assets continue to use their typed
resource owners.

The self-driving frontend test still supplies input through recovered native
handlers and captures the 98 Game Setup, Team Select, and User Setup frames.
After the accepted-controller handoff, its bounded GAMEONLY diagnostic captures
`crc-validator-install-before.ppm` and `crc-validator-install-after.ppm` around
this exact call. They must be pixel-identical to the visible SetDispMask output
because callback registration has no rendering operation. The JSON receipt and
trace prove the retained word changed from zero to `0x800A3D60`.

This is a loader-policy registration step toward match startup, not a file
validation, asset load, court frame, possession, or playable-game claim.
