# GAMEONLY GTE projection-state initialization recovery

`src/recovered/game_gte_initialize.c` owns the complete 26-instruction
GAMEONLY routine `0x80056678..0x800566DF`, reached once from startup call PC
`0x80029A54`. It has no child calls, branches, stack frame, or mapped RAM
accesses. The first effective operation reads CP0 Status, preserves every
existing bit, sets CU2 (`0x40000000`), and writes the result back so subsequent
code may use the PlayStation Geometry Transformation Engine.

The remaining source instructions write these seven GTE control registers:

| Register | Index | Source value | Native role |
| --- | ---: | ---: | --- |
| `OFX` | 24 | `0` | projected screen X offset, signed 16.16 |
| `OFY` | 25 | `0` | projected screen Y offset, signed 16.16 |
| `H` | 26 | `1000` | projection-plane distance |
| `DQA` | 27 | `-4194` | depth-cue interpolation slope |
| `DQB` | 28 | `0x01400000` | depth-cue interpolation offset |
| `ZSF3` | 29 | `0x0155` | three-depth average scale |
| `ZSF4` | 30 | `0x0100` | four-depth average scale |

These are retained projection inputs used later by native court, player, and
net geometry. `ZSF3` and `ZSF4` remain separate, matching the existing geometry
APIs and the earlier complete projection comparisons. This startup routine
does not itself submit a GPU packet or rasterize a pixel.

No repairable gameplay bug was found in the routine. Compatibility still
matters: the source does **not** clear matrices, translation, color controls,
screen/depth FIFOs, `FLAG`, or any of the other 25 GTE control registers. Their
prior values and knownness remain live. Repeated calls overwrite only the same
seven controls, non-CU2 CP0 Status bits survive, and the otherwise-unused `v0`
return remains the updated Status word. The native owner does not replace that
partial initialization with a clean host-side reset.

Public tests cover the exact values, preserved registers, already-enabled CU2,
repeat execution, unknown destination controls, malformed metadata, unknown
Status, every one of the nine register-operation budget stops, prefix writes,
and argument validation. Private bounded R3000 execution compares all 26 source
instructions and the final CP0/GTE register file. The raw routine bytes have
SHA-256 `5c7ab24d5dc0fc9b96402061fba9b8f4548432a62631c889b37a8ec8c07c60c7`;
evidence stays under `.local/verification/native_completion/game_gte_initialize`.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, User
Setup, and the accepted controller assignment through native recovered input
handlers. It captures 98 PPM frames plus logs and records this initializer in
`game_entry_trace.json`. Because this call changes retained projection state
without drawing, its frames should remain byte-identical to the preceding
capture. A natural court/gameplay frame remains a later integration boundary.
