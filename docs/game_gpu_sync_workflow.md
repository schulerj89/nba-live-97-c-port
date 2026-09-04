# GPU synchronization service

`game_gpu_sync.c` is the canonical recovered owner for GAME `800994F4` and
the default `8009B9B4` dispatch closure. It owns `8009BAFC`, `8009B57C`,
`8009BB30`, the reached negative path of `8009BDB4`, and `800986F8` rather
than forwarding the default SDK function to an interpreter or host callback.
Only device accesses, dynamic callbacks, diagnostic calls, and the native
backend completion observer remain typed leaves.

## Wrapper and dynamic dispatch

The wrapper reads the live debug level at `800C55C2`. Values two and above
invoke the live `800C55BC` pointer with text `800282C0` and mode; its return is
ignored. The callback may mutate state. Only after it returns does the wrapper
resolve live `C55B8 + 3C`. Recovered C accepts only the retained target
`8009B9B4`; any other fully known target returns a native dynamic-dispatch
refusal at `80099544`, preserving earlier debug effects.

Native status and source `V0` are separate. A completed source timeout is a
native success with known source `FFFFFFFF`. Unknown data, refused callbacks,
device failures, and budgets return native errors without inventing a source
return or rolling back earlier effects.

## Queue and completion order

Mode zero calls `8009BAFC`, whose reached `8009BDB4(-1)` path performs the
timer-pointer/status/counter/origin reads before returning live `C5574`. It
sets `C56D8 = tick + F0` with 32-bit wrap and clears `C56DC`.

`8009B57C` retains the original order:

- A busy DMA2 CHCR returns one before any I_MASK exchange.
- `800986F8(0)` reads the live I_MASK pointer, reads the old halfword, writes
  zero, and returns the old halfword for `C56D0`.
- Near-full queue state calls typed `8009863C(2,0)` only when `C55CC` is null.
- GPU-ready polling is bounded natively; budget exhaustion is not a source
  timeout and preserves the disabled-mask prefix.
- Each queue handler receives its two retained words. After it returns,
  `C56C8` is reloaded independently for the `C56B4/B8/BC` snapshots and again
  for the consumer increment, preserving handler mutation behavior.
- I_MASK is restored before completion tests. A completion callback can run
  only when the queue is empty, DMA is idle, `C55C8` is nonzero, and `C55CC`
  is non-null; `C55C8` is cleared before the call.
- The returned queue delta is `(C56C4 - C56C8) & 3F` after callback effects.

Mode zero calls `8009BB30` after every queue-drain attempt and between DMA/GPU
polls. Nonzero mode captures the queue delta before an optional drain and
returns that captured value whenever nonzero. An empty nonzero query returns
zero only when DMA is idle and GPU ready, otherwise one.

## Timeout/reset quirk

`8009BB30` first calls reached `8009BDB4(-1)`. It uses signed comparisons for
`C56D8 < now` and `000F0000 < old C56DC`; the poll counter is stored as
`old + 1` before its threshold comparison. The original reset order is
retained: two `8009CB2C` diagnostics; `800986F8(0)`; consumer zero; saved mask;
producer zero from the just-zeroed consumer; DMA2 CHCR `00000401`; full-word
DPCR read/OR `00000800`/write; GPUSTAT `02000000`; GPUSTAT `01000000`; and
I_MASK restoration. The known source result is `FFFFFFFF`.

This includes original oddities: the first timeout GPUSTAT read is discarded,
the second is passed to the diagnostic, queue diagnostics use stale
`C56B4/B8/BC`, and non-negative `8009BDB4` behavior is not claimed.

## Native device-integrity fence

The source return alone cannot prove that a host renderer completed work. For
mode zero, a typed native observer records monotonic submission/completion
serials after debug dispatch and again after a known source zero. Acceptance
requires an idle backend with every submitted serial complete, including work
submitted by queue handlers during the call. This metadata does not alter PS1
RAM or source `V0`. A fake synchronous acknowledgement therefore yields
`NBA97_GAME_GPU_SYNC_DEVICE_INCOMPLETE`, while delayed DMA/GPU readiness keeps
the recovered source loop active until the deterministic backend completes.

## Natural GAMEONLY startup composition

GAMEONLY calls this owner at `0x80029AAC` with mode zero, immediately after the
two `MoveImage` calls at `0x80029A94` and `0x80029AA4`. The native game-entry
diagnostic now connects that exact boundary instead of acknowledging it as an
opaque callback. Its retained backend deliberately leaves both VRAM-copy
packets pending. `DrawSync` observes two submitted and zero completed packets,
samples DMA2 busy once, performs the original timeout-accounting reads, polls
DMA2 and GPU readiness again, then accepts the second backend observation only
after both copies are complete.

The optional `Nba97GameGpuSyncAbi` mapping also retains `800994F4`'s real
24-byte o32 frame. Incoming `s0` and `ra` are stored at the original addresses
and reloaded from live mapped bytes, including callback mutations. Existing
standalone users may leave this mapping null.

The self-driving visual test writes `draw-sync-before-buffer0.ppm` while the
two packets are still pending and `draw-sync-after-buffer0.ppm` after the
synchronization returns. The former matches the original flat destination;
the latter matches the generated source grid and both final framebuffer pages.
This fixture is diagnostic data, not recovered retail art, and it does not
replace or mutate the native frontend renderer.

## Verification and claim boundary

Private evidence under `.local/verification/native_completion/game_gpu_sync/`
audits 437 retained words across eight claimed intervals. The original-CPU
comparison covers all 4,096 queue head/tail pairs, debug levels, wrap/drain,
handler mutation, completion/wait, delayed device readiness, both mode paths,
timeout reset, and dynamic refusal; all 437 claimed PCs are reached. A separate
10,000-case backend oracle exercises delayed submission/completion and rejects
fake acknowledgements. Strict tests pass MSVC Debug/Release and GCC/UBSan.

The component now has a natural source-caller trace in the bounded game-entry
diagnostic. There is still no physical PS1 timing claim, live production GPU
connection, rendered match frame, court, first possession, or gameplay claim.
