# Pregame match-card recovery

This module owns GAMEONLY `0x80044550..0x80044997`, 1096 bytes and 274
instructions. The fresh Ghidra listing has SHA-256
`ceffd8479963e8b590f58996f88f2e95d737f1412a272a90a31c82d561c1d834`.
Its sole observed caller is the recovered period-presentation finisher at
`0x8002DDF8`, with NOP delay, return address `0x8002DE00`, and no explicit
arguments.

The source creates a 0x68-byte live stack frame, saves `ra` and `s4..s0`, and
issues 37 typed service calls in source order. The listing contains seven
`0x80031614` layout calls and eight `0x80030D18` text calls. Stack arguments,
font-mode writes, the two-byte stack string, team pointers, the full-word
location gate, signed low-half location argument, and every callback mutation
remain live. Guest text pointers stay mapped PS1 addresses; the recovery does
not copy retail strings or tables into native fixtures.

After creating the card, the source optionally starts demo presentation audio,
captures an initial clock, and obtains two signed controller halves. Each poll
pumps a frame, reads input, samples the clock, checks readiness, and calculates
the wrapping clock delta. A zero readiness result resets `s1`; a nonzero result
only accumulates once `s1` is nonnegative. Reaching 480 assigns `s2=3610`.
Positive signed deltas add to `s2` in the `0x800448F8` call delay and run the
timing/audio services. Input bits `0x180` exit immediately; unrelated input bits
do not. The signed 3600 gate provides the timeout exit.

On exit, any full nonzero input runs sound 97. When the demo flag is nonzero,
the source stores one to `0x800FDB78` and 99 to `0x8001EDEC`. It then publishes
the signed second controller half, closes the UI service, clears
`0x800EB680`, runs cleanup, and reloads all saved registers through callback-live
`sp`. The operation budget counts each attempted retained-memory access and
typed call, preserving the exact reached prefix without inventing timer
progress.

The native adapter claims the period finisher's assigned kind, PC, delay PC,
entry, or known return address before exact validation. Its wrapper composes the
actual recovered parent and leaves the separate `0x80046C2C` child on the
parent's typed fallback. Focused fixtures exercise call order and arguments,
location/demo/input/timer/readiness paths, every call kind refusal, all operation
cutoffs, unknown branch prefixes, callback-live frame relocation, and epilogue
return failures. It also pins the bounded unchanged-clock/readiness-one polling
prefix, the font-write alias into the saved-return-address slot, and every
untouched GPR word and known mask. The focused suite passes 743 checks, and the
natural fixture
runs the same adapter through the actual period-presentation owner on shared
retained memory with 248 passing checks. The manager's independent differential
passes 11,584 cases across all 274 source PCs, all 34 machine words and masks,
the full 2 MiB retained image, callback machines and 0x68-byte stack snapshots,
mutable saved registers/sp, and access/call cutoffs 0 through 169.

Gameplay shown: BLOCKED. This routine describes UI/menu presentation, but its
typed `0x80031614` layout, `0x80030D18` text, `0x80036688` text-buffer,
`0x8009CB6C` team-name copy, `0x80081B50` location lookup, and `0x8009CB7C`
location-format services are not yet composed with a recovered renderer in this
assignment. The font pointer at `0x800B2048` also remains retained CPU state.
Synthetic service fixtures do not constitute rendered UI or gameplay.

Manager validation: strict Clang C99/C++17 and VS2022 focused 743 / natural BZ 248 checks; full asset-free CTest 353/353. Private original comparison passed 11,584 cases across all 274 instructions, comparing all 34 machine words, complete known masks, 2 MiB RAM, callback entry machines and stack arguments, relocated frames and bounded prefixes.
Native self-driving run game-entry-20260906-071149-a59e2e7f composes the actual presentation caller and card on the same synthetic retained memory. Clock/readiness/stream pump/status execute their existing owners; explicit clock 100 stays unchanged and typed input 0x180 exits one poll. Seven layout and eight text callbacks are observed. UI drawing, resource services, and frame rendering remain typed dependencies. Gameplay shown: BLOCKED at 0x80031614 / 0x80030D18 / 0x80049018. Visible native screen is User Setup, with ignored local PNG proof; no pregame card or advancing match is claimed. Native CPU before/after SHA256 is 391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d. Captures are ignored and never committed.
