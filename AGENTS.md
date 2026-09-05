# NBA Live 97 C-port agent instructions

These instructions apply to the entire repository. They define the default
workflow for continuing the PlayStation-to-native-C recovery one original
subroutine at a time.

## Non-negotiable project rules

- Follow every applicable instruction in this file for every change. If a user
  request conflicts with it, stop and identify the conflict before changing
  code.
- Recover exactly one previously unowned original PS1 subroutine per recovery
  commit. Its directly related header, native adapter, tests, build registration,
  documentation, and generated textual progress metadata may accompany it.
- Do not quietly implement a second original subroutine in the same commit.
  Keep unresolved callees behind typed callbacks or existing recovered owners
  so they can be handled by later commits.
- Complete the selected subroutine's entire evidenced address range. Do not
  label a partial path or high-level approximation as a complete translation.
- Preserve original-game behavior, including known bugs, surprising return
  values, signedness, overflow, wraparound, delay-slot effects, access order,
  mutable registers, and failure behavior. Do not clean up or repair the source
  semantics inside the recovered owner.
- Keep the application main loop light. It may coordinate input, timing,
  recovered owners, host services, and presentation; it must not contain the
  translated algorithm or grow into a second implementation of it.
- Never commit assets. This includes retail inputs, executables, overlays,
  extracted data, generated recompilation/decompilation output, ROM/disc bytes,
  binary fixtures, images, screenshots, frame captures, video, audio, fonts,
  textures, models, likenesses, emulator dumps, saves, or assets encoded into
  source as byte arrays/base64. Keep all such material in ignored local paths.
- Never force-add an ignored file. Never force-push.
- Preserve unrelated user changes and stashes. A routine commit must contain
  only work required for that routine.

The commit that first adds this `AGENTS.md` is the user-requested bootstrap
exception to the one-subroutine rule. After that bootstrap commit, documentation
or infrastructure-only commits require an explicit user request.

## Selecting and proving one source boundary

1. Start from the next reachable unresolved jump/call boundary on the current
   recovery path unless the user selects another target.
2. Record the source program or overlay as well as the address, because PS1
   overlays can reuse virtual addresses.
3. Establish the exact inclusive address range, byte size, instruction count,
   callers, direct callees, inputs, returns, global memory effects, and relevant
   delay slots from fresh evidence. Use the recompilation corpus, read-only
   Ghidra analysis, no$psx traces, or a combination of them.
4. Cross-check ambiguous control flow or data semantics with a second evidence
   source when possible. Never infer a completion claim from behavior alone.
5. Keep private source bytes, decompiler listings, traces, screenshots, and
   extracted files under `.local/`. Commit only human-authored source,
   asset-free tests, source metadata, and documentation.
6. Before implementation, check whether the address is already completely
   owned elsewhere. Prefer composition over duplicating a recovered routine.

## PS1 address architecture and C comment contract

Treat a PS1 address as a 32-bit guest virtual address, never as a native host
pointer. Keep the exact eight-digit virtual address (`0x8002DB90`, for example)
in provenance and diagnostics. Guest reads and writes must go through the
module's validated retained-memory/context abstraction. Do not cast a guest
integer directly to a host pointer. Preserve little-endian loads/stores, 32-bit
arithmetic, signed MIPS comparisons, alignment behavior, and KSEG address bits
explicitly. Use `uint32_t`/`int32_t` or an existing guest-address type and use
`UINT32_C(...)` where it prevents host-width ambiguity.

Every translated C subroutine must have the following comment immediately
above its public declaration or implementation. Keep every field; write
`None observed` when a field genuinely has no content.

```c
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY | FELOAD | FEONLY | GAMELOAD | <other overlay>
 * Address: 0x80000000
 * Range: 0x80000000..0x80000003 (inclusive)
 * Source size: 4 bytes / 1 instruction
 * Evidence: <recomp, Ghidra, and/or no$psx evidence and stable hash/trace ID>
 *
 * Purpose: <one-sentence source-level role>
 * Inputs: <MIPS argument registers, live registers, and guest-memory inputs>
 * Returns: <v0/v1 and any required live-register state>
 * Guest memory: <read/write addresses, ranges, ownership, and ordering>
 * Calls: <direct/indirect targets in original source order>
 * Original quirks: <bugs, traps, wraparound, odd returns, or None observed>
 * Native mapping: <context/callback/owned-buffer mapping; no host-pointer cast>
 */
```

Within the function, annotate each non-obvious translated block with its source
PC or inclusive PC range. Label original call PCs and delay-slot work where
ordering matters. Comments must explain the relationship to the PS1 routine,
not merely restate the C expression. Prefer a descriptive native function name;
the original address belongs in the mandatory comment and evidence logs rather
than being the only meaning carried by the identifier.

## Implementation boundaries

- Put source-faithful owners in `src/recovered/` as C99 unless an established
  module layout requires otherwise. Put host rendering, audio, filesystem, or
  platform adaptation outside the recovered owner.
- Model unresolved original callees with typed dependency callbacks containing
  their PS1 target address and call-site PC. Do not fabricate their result.
- Reuse an existing recovered owner through a narrow adapter when the source
  calls it. Do not copy its algorithm into the new owner or the application
  loop.
- Make original execution order observable where it matters. Tests should be
  able to inspect calls, guest accesses, return state, traps, and completed
  prefixes without parsing console prose.
- Safety limits may bound an original runaway loop for the host test harness,
  but the result must explicitly report that bounded condition and preserve the
  exact observable prefix. A safety wrapper is not permission to change normal
  source behavior.
- Do not claim that a synthetic fixture is a retail payload, that a rendered
  diagnostic pose is gameplay, or that semantic equivalence is instruction-
  identical recompilation.

## Required tests for every recovered subroutine

Every routine commit must include or update an asset-free registered unit test
that executes the new C owner directly. A natural-caller integration test is
also required when that caller is already recovered and can be composed without
implementing another subroutine.

Tests must cover, as applicable:

- the normal path and every control-flow exit;
- argument/register forwarding, source call order, call PCs, and delay slots;
- return registers and live-register behavior relied on by callers;
- exact guest-memory read/write ranges and material access ordering;
- zero, boundary, signed, wraparound, alignment, overlap, trap, unknown-data,
  failure-prefix, and bounded-runaway cases present in the source;
- the original bug or quirk rather than a corrected expectation; and
- deterministic repeatability with no retail asset dependency.

Use synthetic fixtures generated by test code at runtime. Do not check in a
binary fixture or copied retail table. When private original-game evidence is
available, a local differential or no$psx comparison should additionally prove
the same cases, but CI and the committed unit test must remain asset-free.

Register the focused test with CTest and run it directly first. Before commit,
run the relevant integration/visual verifier, the complete asset-free CTest
suite, `python tools/report_progress.py --check`, and any metadata freshness
check affected by the change. A routine is not complete while required tests
are failing.

## Self-driving visual, menu, and gameplay evidence

If the routine is reachable from the native frontend or match path, update the
existing visual test to drive that path itself. The test must inject controller,
keyboard, or pointer events through the port's own input/event APIs. Do not use
computer-control automation, OS-level mouse automation, or manually clicked
steps as test evidence.

The visual run must capture deterministic logs and frames natively:

- log the routine program, address/range, scripted input steps, frame numbers,
  recovered call/state checkpoints, and before/after frame hashes;
- capture a frame immediately before the relevant transition and the first
  stable frame after it; capture an additional gameplay frame after state has
  advanced when gameplay is genuinely running;
- assert the expected pixel change for UI, menu, or rendered gameplay work;
  for a CPU-only routine, assert pixel-identical frames and prove the state or
  memory change in logs instead;
- distinguish `UI/menu`, `gameplay`, and `no direct visual effect`; and
- never call a fixture-only pose, static court diagnostic, loading screen, or
  menu transition "gameplay." Gameplay requires an advancing native match loop
  and a rendered court/player state produced by that path.

Write logs and frame captures only beneath an ignored build directory or
`.local/evidence/<program>-<address>/`. Screenshot proof is local run evidence:
show the resulting absolute-path images in the final handoff whenever the run
shows UI/menu or gameplay, but never stage or commit those images. Commit the
test code and asset-free textual expectations, not its captured media.

Every final handoff for a routine must explicitly report one of:

- `Gameplay shown: YES` and include native screenshot proof;
- `Gameplay shown: NO - UI/menu only` and include native screenshot proof;
- `Gameplay shown: NO - no direct visual effect`, with matching frame hashes
  plus the state/log evidence; or
- `Gameplay shown: BLOCKED`, with the exact missing boundary or dependency.

## Keep the main loop light

- The application loop should poll/translate input, advance one bounded update,
  invoke recovered orchestration through narrow interfaces, and present.
- No recovered algorithm, guest-memory parser, asset decoder, lengthy state
  machine, differential harness, or screenshot encoder belongs inline in the
  main loop.
- Avoid per-frame filesystem work, asset loading, heap churn, verbose proof
  logging, or synchronous capture processing. Gate diagnostics and perform
  capture encoding outside the hot update/present path.
- When integration would make `win32_main.cpp` materially larger, add a small
  module with a typed interface and leave only ownership and dispatch in main.
- Tests must call the same production owner/adapter used by the application;
  do not create a test-only reimplementation of the translated routine.

## Commit and push checklist

Before creating the single-routine commit:

1. Inspect `git status` and preserve unrelated work.
2. Review the staged diff and staged file list. Confirm that it contains one new
   recovered subroutine, its focused support/tests/docs, and no assets.
3. Confirm all capture/log outputs are ignored and that no file from `.local/`
   or a build directory is staged.
4. Run `git diff --cached --check` and the required verification commands.
5. Use a concise commit subject such as `Recover GAMEONLY match initializer`;
   describe the PS1 address and tests in the body when useful.
6. Push the current branch normally to its configured upstream. Do not amend,
   rewrite, or force-push unrelated history.

The final handoff must name the recovered routine and source range, summarize
what it does and how it is wired into the port, list preserved quirks, report
focused and full-suite test results, provide the required gameplay/UI visual
classification and proof, and give the commit hash and push result.
