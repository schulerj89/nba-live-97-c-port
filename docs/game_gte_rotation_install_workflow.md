# GTE rotation installation recovery

`game_gte_rotation_install.c` owns the complete GAMEONLY routine `80055F18..80055F43`. The eleven source instructions read five consecutive matrix words through a raw guest pointer in A0, retain the raw values in T0 through T4, and then write GTE controls 0 through 4. The owner carries all 32 GPRs, HI/LO, mapped guest memory, per-byte knowledge, and a separate 32-word byte-known GTE control bank.

All five `LW` instructions finish before the first `CTC2`. This ordering is visible through the access and control journals and through every operation-budget prefix. Controls 0 through 3 receive the raw 32-bit words and masks. Control 4 is RT33: the hardware write keeps the low half and sign-extends bit 15 through the upper half. Its upper-byte knowledge therefore follows source byte 1, while T4 retains the unmodified raw word and mask.

The last `CTC2` is the delay slot of `JR RA`. It executes before an unknown return address refuses native continuation. AY does not change SP or guest memory, clear FLAG or FIFOs, execute a projection, or invent a V0 return. Every other GPR, HI/LO, and GTE control remains live.

`game_gte_rotation_install_adapter.cpp` binds AY to the existing camera-frame-transform event at source PC `80051204`, delay PC `80051208`, entry `80055F18`, one argument, and fully known RA `8005120C`. The camera event already carries the same full machine and retained memory. The adapter owns an independent GTE control bank, copies the complete machine prefix back on owner success or failure, and forwards the other camera children through their typed callback.

The focused asset-free tests cover the five-read/five-control order, all register and control preservation, raw T0â€“T4 values, RT33 values `0000`, `7FFF`, `8000`, and `FFFF` with upper garbage, partial masks, budgets 0 through 10, the JR-delay prefix, unknown and invalid pointers, alignment, mapping, wrapped address addition, aliased native backing, malformed metadata, and deterministic repetition. The integration test executes the actual recovered AQ owner and proves AY completes before AQ reaches the following translation boundary.

Gameplay shown: NO - no direct visual effect. This routine changes CPU-visible GTE control state used by later projection; it does not submit or render pixels itself.

## Manager integration and native evidence

The natural camera caller composes the existing recovered rotation-matrix
builder at 0x80051168 before calling this installer at 0x80051204. The two
owners share the caller's mapped memory and full machine. A runtime-generated
packed trigonometric table supplies synthetic inputs; translation/reference
services remain explicit typed dependencies. Native capture asserts the five
installed controls, raw T4 upper bits, unknown untouched controls, returned SP
and RA, and matching CPU-only frame hashes. This proves installation after
the caller's matrix scaling, not rendered gameplay.

Private raw-source comparison covers all 11 instructions in 5,632 cases,
including all machine/control masks and operation budgets. Captures and private
original evidence remain ignored.
