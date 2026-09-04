# GAMEONLY double-buffer video-environment recovery

`src/recovered/game_video_environment_initialize.c` owns the complete
94-instruction GAMEONLY routine `0x80029F20..0x8002A097`. Startup reaches it
at call PC `0x80029A6C`; the delay slot at `0x80029A70` supplies mode `0`.
The other three original callers (`0x8002C3C0`, `0x8002D204`, and
`0x8002E1D8`) supply mode `1`, so the owner retains the full input and applies
the source's low-byte stores rather than hard-coding the startup case.

The function creates the PS1 game's two vertically interleaved buffer pairs:

| Role | Address | Rectangle `(x,y,w,h)` |
| --- | --- | --- |
| Display 0 | `0x8002205C` | `(0,256,512,240)` |
| Display 1 | `0x80022070` | `(0,0,512,240)` |
| Draw 0 | `0x80021EEC` | `(0,0,512,240)` |
| Draw 1 | `0x80021F48` | `(0,256,512,240)` |

Those opposite pages let one image be displayed while the next image is
drawn. The source then calls `PutDispEnv`/`PutDrawEnv` for pair 0 followed by
pair 1, executes `DrawSync(0)`, and clears software buffer selector
`0x8001EDE8`. The final installed hardware pair is therefore pair 1 while the
software selector is zero; the next frame owner toggles that selector before
installing an environment again.

The nine PsyQ calls remain explicit synchronous boundaries:

| Calls | Original entry | Native event |
| ---: | --- | --- |
| 2 | `0x8009CAD0` | `SetDefDispEnv` |
| 2 | `0x8009CA00` | `SetDefDrawEnv` |
| 2 | `0x80099CA4` | `PutDispEnv` |
| 2 | `0x80099ACC` | `PutDrawEnv` |
| 1 | `0x800994F4` | `DrawSync(0)` |

The owner models its real 56-byte stack frame. In particular, each SetDef
call's fifth argument (`240`) is stored at `sp+0x10` by the preceding JAL delay
slot before the callback runs. All seven saved words are reloaded from live
mapped memory in source order, allowing a child alias to remain observable.
The incidental return is the final `DrawSync` result. A complete startup run
has 44 operations: 35 mapped accesses (28 stores and seven reads) plus nine
completed child calls.

Compatibility quirks are not repaired:

- The mode argument is copied with `sb`, so only its low byte reaches `isbg`.
- `dtd` and `isbg` are written in four consecutive DRAWENV records even
  though only the first two were passed to `SetDefDrawEnv`.
- RGB bytes are cleared only in the two initialized DRAWENV records; the two
  adjacent records retain their previous RGB and `dfe` bytes.
- Both pairs are installed, leaving pair 1 active while the selector is zero.
- No other bytes in those records are sanitized, and all child effects remain
  prefix-committing if a later boundary fails.

The standalone test covers the exact rectangles, child order and arguments,
mode `0`, mode `1`, arbitrary low-byte truncation, untouched-byte sentinels,
active-pair/selector mismatch, unknown and malformed child results, live stack
rewrites, every one of the 44 operation-budget stops, all nine refusal points,
mapped-memory faults, and argument validation. The startup composition test
proves the natural `0x80029A6C` call uses mode zero.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through the native recovered input handlers; it does not use computer
control. It captures 98 PPM frames, writes the semantic log, and records this
owner in `game_entry_trace.json`. This routine configures retained PS1
environment metadata but submits no scene primitives, so its correct visual
effect is **none**: the captured frontend pixels remain unchanged while the
receipt proves the four rectangles, nine child boundaries, final active pair,
and selector value.

Ghidra's extracted body has raw-instruction SHA-256
`6f76725da5861e858ac9cd7f37081f1252c9472ee3d305546d44424bbcbe5c3b`.
Listings, decompilation, all four callers, and child-boundary evidence remain
under `.local/verification/native_completion/game_call_80029f20`.
