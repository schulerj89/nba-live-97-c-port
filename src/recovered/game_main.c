#include "game_main.h"

#include <string.h>

typedef struct Nba97GameMainRun {
    Nba97GameMainContext* context;
    Nba97GameMainProgress* out;
    uint32_t sp;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
} Nba97GameMainRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameMainRun* run, uint32_t pc, uint32_t address,
    uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameMainRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameMainRun* run, uint32_t address, size_t width,
    size_t alignment, uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address, 0);
    TRY(spend(run));
    ++run->out->accesses;
    if (address & (uint32_t)(alignment - 1u))
        return NBA97_TEXT_ALIGNMENT_TRAP;
    for (i = 0; i < run->context->memory.count; ++i) {
        Nba97GameTextRegion* region = &run->context->memory.region[i];
        uint64_t offset = (uint64_t)address - region->base;
        if (address < region->base || offset > region->size ||
            width > region->size - (size_t)offset)
            continue;
        *data = region->data + (size_t)offset;
        *known = region->known ? region->known + (size_t)offset : 0;
        if (*known)
            for (j = 0; j < width; ++j)
                if ((*known)[j] > 1)
                    return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}

static int write_value(Nba97GameMainRun* run, uint32_t address, size_t width,
    uint32_t pc, uint32_t value) {
    uint8_t* data;
    uint8_t* known;
    size_t i;
    TRY(locate(run, address, width, width, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = 1;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameMainRun* run, uint32_t address, uint32_t pc,
    uint32_t value) {
    return write_value(run, address, 4, pc, value);
}

static int write_half(Nba97GameMainRun* run, uint32_t address, uint32_t pc,
    uint16_t value) {
    return write_value(run, address, 2, pc, value);
}

static int read_word(Nba97GameMainRun* run, uint32_t address, uint32_t pc,
    uint32_t* value) {
    uint8_t* data;
    uint8_t* known;
    uint32_t result = 0;
    unsigned i;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    if (known)
        for (i = 0; i < 4; ++i)
            if (!known[i])
                return NBA97_TEXT_UNKNOWN;
    for (i = 0; i < 4; ++i)
        result |= (uint32_t)data[i] << (i * 8u);
    *value = result;
    ++run->out->reads;
    return NBA97_TEXT_COMPLETE;
}

static int validate(Nba97GameMainContext* context, Nba97GameMainProgress* out,
    Nba97GameMainRun* run) {
    size_t i;
    size_t j;
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || (!context->memory.region && context->memory.count))
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < context->memory.count; ++i) {
        const Nba97GameTextRegion* a = &context->memory.region[i];
        if (!a->data || !a->size || a->size > UINT64_C(0x100000000) ||
            (uint64_t)a->base + a->size > UINT64_C(0x100000000))
            return NBA97_TEXT_ARGUMENT;
        for (j = 0; j < i; ++j) {
            const Nba97GameTextRegion* b = &context->memory.region[j];
            if ((uint64_t)a->base < (uint64_t)b->base + b->size &&
                (uint64_t)b->base < (uint64_t)a->base + a->size)
                return NBA97_TEXT_ARGUMENT;
        }
    }
    run->context = context;
    run->out = out;
    run->sp = context->stack_pointer - 0x28u;
    run->s0 = context->saved_register[0];
    run->s1 = context->saved_register[1];
    run->s2 = context->saved_register[2];
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    return NBA97_TEXT_COMPLETE;
}

static int invoke(Nba97GameMainRun* run, uint32_t pc, uint32_t entry,
    uint8_t kind, uint8_t argument_count, uint32_t a0, uint32_t a1,
    uint32_t a2, Nba97GameMainValue* value,
    enum Nba97GameMainCalleeOutcome* outcome) {
    Nba97GameMainEvent event;
    int result;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    if (kind == NBA97_GAME_MAIN_INDIRECT_CALL && (entry & 3u))
        return NBA97_TEXT_ALIGNMENT_TRAP;
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.entry = entry;
    event.argument[0] = a0;
    event.argument[1] = a1;
    event.argument[2] = a2;
    event.stack_pointer = run->sp;
    event.global_pointer = run->context->global_pointer;
    event.saved_register[0] = run->s0;
    event.saved_register[1] = run->s1;
    event.saved_register[2] = run->s2;
    event.return_address = pc + 8u;
    event.kind = kind;
    event.argument_count = argument_count;
    value->word = 0;
    value->known = 0;
    *outcome = NBA97_GAME_MAIN_CALLEE_UNSET;
    result = run->context->io(run->context->user, &run->context->memory,
        &event, value, outcome);
    if (result != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (*outcome != NBA97_GAME_MAIN_CALLEE_RETURNED &&
        *outcome != NBA97_GAME_MAIN_CALLEE_TRANSFERRED)
        return NBA97_TEXT_ARGUMENT;
    if (kind == NBA97_GAME_MAIN_DIRECT_CALL &&
        *outcome != NBA97_GAME_MAIN_CALLEE_RETURNED)
        return NBA97_TEXT_ARGUMENT;
    if (value->known > 1)
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    return NBA97_TEXT_COMPLETE;
}

static int direct_call(Nba97GameMainRun* run, uint32_t pc, uint32_t entry,
    uint8_t argument_count, uint32_t a0, uint32_t a1, uint32_t a2,
    Nba97GameMainValue* value) {
    enum Nba97GameMainCalleeOutcome outcome;
    return invoke(run, pc, entry, NBA97_GAME_MAIN_DIRECT_CALL, argument_count,
        a0, a1, a2, value, &outcome);
}

int nba97_game_main(Nba97GameMainContext* context, Nba97GameMainProgress* out) {
    Nba97GameMainRun storage;
    Nba97GameMainRun* run = &storage;
    Nba97GameMainValue value;
    enum Nba97GameMainCalleeOutcome outcome;
    uint32_t indirect;
    unsigned i;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80029994 prologue. The s0 save is the delay slot of the
     * 0x800948D0 call and therefore completes before that callee observes RAM. */
    TRY(write_word(run, run->sp + 0x24u, 0x80029998u, context->return_address));
    TRY(write_word(run, run->sp + 0x20u, 0x8002999cu, run->s2));
    TRY(write_word(run, run->sp + 0x1cu, 0x800299a0u, run->s1));
    TRY(write_word(run, run->sp + 0x18u, 0x800299a8u, run->s0));
    TRY(direct_call(run, 0x800299a4u, 0x800948d0u, 0, 0, 0, 0, &value));
    run->s0 = 1;
    TRY(direct_call(run, 0x800299acu, 0x800a4830u, 0, 0, 0, 0, &value));
    TRY(direct_call(run, 0x800299c8u, 0x8008fa6cu, 3, 0xdcu,
        0x8010b61cu, 0x000f21e4u, &value));
    TRY(write_word(run, 0x800d7b04u, 0x800299d4u, 0));
    TRY(direct_call(run, 0x800299d8u, 0x80091c08u, 0, 0, 0, 0, &value));
    TRY(direct_call(run, 0x800299e8u, 0x800a35d8u, 1, 0x800247e4u, 0, 0, &value));
    TRY(direct_call(run, 0x800299f8u, 0x80092c7cu, 2, 0x8001000cu, 0x2c3u, 0, &value));
    TRY(write_half(run, 0x8002148cu, 0x80029a04u, 0));
    /* GAMEONLY startup clears the inherited PsyQ mask first, then enters the
     * separately recovered ResetCallback table-dispatch wrapper. Neither call
     * is treated as a native host interrupt operation by this composition. */
    TRY(direct_call(run, 0x80029a08u, 0x800985b4u, 1, 0, 0, 0, &value));
    TRY(direct_call(run, 0x80029a10u, 0x800985dcu, 0, 0, 0, 0, &value));
    /* GAMEONLY 0x80029A18 -> 0x8008F1D4 is the separately recovered
     * controller-resume/mode owner. With the retail suspend flag initially
     * one, this mode-8 call initializes pad sampling and snapshots its clock. */
    TRY(direct_call(run, 0x80029a18u, 0x8008f1d4u, 1, 8, 0, 0, &value));
    /* GAMEONLY 0x80029A20 -> 0x80099058 is the recovered PsyQ
     * ResetGraph owner. Mode 3 takes its initialization path without clearing
     * video RAM; GPU/BIOS service calls remain explicit child boundaries. */
    TRY(direct_call(run, 0x80029a20u, 0x80099058u, 1, 3, 0, 0, &value));
    /* GAMEONLY 0x80029A28 -> 0x800992C4 is PsyQ SetGraphDebug. Level zero
     * publishes a disabled debug byte and returns the prior byte without
     * invoking its deliberately unguarded diagnostic function pointer. */
    TRY(direct_call(run, 0x80029a28u, 0x800992c4u, 1, 0, 0, 0, &value));
    /* GAMEONLY calls the same owner again at 0x80029A30. Input is already
     * active here, so source behavior still writes mode 8 but skips children. */
    TRY(direct_call(run, 0x80029a30u, 0x8008f1d4u, 1, 8, 0, 0, &value));
    /* GAMEONLY 0x80029A38 -> 0x800A43E8 is the recovered VBlank-service
     * initializer. Its PS1 interrupt/counter calls remain explicit children;
     * the owner clears eight callback slots and preserves counter-3 quirks. */
    TRY(direct_call(run, 0x80029a38u, 0x800a43e8u, 0, 0, 0, 0, &value));
    TRY(write_word(run, 0x800d7a94u, 0x80029a48u, 0x78u));
    /* GAMEONLY 0x80029A4C -> 0x800914D8 is the recovered source-clock
     * initializer. It installs IRQ6 once, derives Timer 2's target from the
     * live stack-spilled rate, preserves the original divide BREAK paths, and
     * resets clock counters after starting the counter. Hardware/service calls
     * stay explicit child boundaries; this call does not invent host cadence. */
    TRY(direct_call(run, 0x80029a4cu, 0x800914d8u, 1, 0x78u, 0, 0, &value));
    /* GAMEONLY 0x80029A54 -> 0x80056678 enables GTE/CU2 while preserving
     * every other CP0 Status bit, then installs the seven original projection
     * controls. It intentionally leaves the other 25 GTE controls live and
     * returns the updated Status word; this caller ignores that raw return. */
    TRY(direct_call(run, 0x80029a54u, 0x80056678u, 0, 0, 0, 0, &value));
    /* GAMEONLY 0x80029A5C -> 0x800A584C samples 0x800A5810, replaces the
     * gp+0x164 clock baseline, and returns raw modulo-2^32 elapsed ticks. The
     * initializer immediately before it reset both clock words, so natural
     * startup returns zero; this caller deliberately ignores that value. */
    TRY(direct_call(run, 0x80029a5cu, 0x800a584cu, 0, 0, 0, 0, &value));
    /* GAMEONLY 0x80029A64 -> 0x80029BDC is the recovered presentation-wait
     * wrapper. It saves live ra and delegates to source synchronization leaf
     * 0x800A9CC0; that leaf remains an explicit service boundary rather than
     * being replaced with host sleep or renderer cadence. The same wrapper is
     * deliberately reused by both twenty-call FELOAD delay loops below. */
    TRY(direct_call(run, 0x80029a64u, 0x80029bdcu, 0, 0, 0, 0, &value));
    /* GAMEONLY 0x80029A6C -> 0x80029F20 is the recovered double-buffer
     * environment initializer. It builds 512x240 display/draw pairs on
     * opposite VRAM pages, installs both, DrawSyncs, and clears 0x8001EDE8.
     * Its asymmetric writes into two otherwise-uninitialized DRAWENVs and
     * low-byte background-mode truncation are preserved by that owner. */
    TRY(direct_call(run, 0x80029a6cu, 0x80029f20u, 1, 0, 0, 0, &value));

    /* GAMEONLY 0x80029A74..0x80029AA8 builds RECT(512,0,512,256) in
     * this live stack frame, then calls the recovered PsyQ MoveImage owner
     * 0x800997E4 twice. The JAL delay slots finish the rectangle height for
     * the first call and supply destination y=256 for the second. Together
     * they copy the staged right-hand VRAM page to both framebuffer pages;
     * zero/negative extent quirks and shared packet behavior stay in that
     * owner rather than being normalized here. */
    TRY(write_half(run, run->sp + 0x10u, 0x80029a84u, 0x200u));
    TRY(write_half(run, run->sp + 0x14u, 0x80029a88u, 0x200u));
    TRY(write_half(run, run->sp + 0x12u, 0x80029a90u, 0));
    TRY(write_half(run, run->sp + 0x16u, 0x80029a98u, 0x100u));
    TRY(direct_call(run, 0x80029a94u, 0x800997e4u, 3, run->sp + 0x10u,
        0, 0, &value));
    TRY(direct_call(run, 0x80029aa4u, 0x800997e4u, 3, run->sp + 0x10u,
        0, 0x100u, &value));
    /* GAMEONLY 0x80029AAC -> 0x800994F4 is the recovered PsyQ DrawSync
     * owner. Mode zero waits until the two MoveImage GPU submissions above,
     * the SDK queue, DMA2, and GPU readiness are complete. Its live debug
     * callback/table reload, signed timeout/reset oddities, raw v0, and o32
     * stack reload remain source behavior rather than host normalization. */
    TRY(direct_call(run, 0x80029aacu, 0x800994f4u, 1, 0, 0, 0, &value));
    /* GAMEONLY 0x80029AB4 -> 0x80099458 is the recovered PsyQ SetDispMask
     * owner. Argument one selects active-low GP1(03h) word 0x03000000 and
     * enables scanout only after both staged framebuffer copies are complete.
     * Its debug callback, disable-only cache clear, live table slot +0x10,
     * raw child v0, and unguarded dispatch behavior remain source-visible. */
    TRY(direct_call(run, 0x80029ab4u, 0x80099458u, 1, 1, 0, 0, &value));
    /* GAMEONLY 0x80029ABC -> 0x800A3E20 is the recovered resource-validator
     * installer. It unconditionally stores whole-file CRCF callback
     * 0x800A3D60 at 0x800D7B1C and incidentally leaves that address in v0.
     * The callback body is a separate boundary; this call neither validates
     * a file nor repairs its ignored-trailer-length source quirk. */
    TRY(direct_call(run, 0x80029abcu, 0x800a3e20u, 0, 0, 0, 0, &value));
    TRY(write_word(run, 0x800d7af4u, 0x80029ac8u, 0));
    TRY(write_word(run, 0x800d7af8u, 0x80029ad0u, run->s0));
    /* GAMEONLY 0x80029AD4 -> 0x800A7738 is the recovered frame-rate tracker
     * reset. It clears five GP-relative words, then samples retained clock
     * leaf 0x800A5810 into baseline 0x800D7B4C before match orchestration.
     * Its pre-callback clear order, unguarded sample store, incidental v0 and
     * live ra reload remain observable; it creates no host timer or pixels. */
    TRY(direct_call(run, 0x80029ad4u, 0x800a7738u, 0, 0, 0, 0, &value));
    out->reached_match_orchestration = 1;
    /* GAMEONLY 0x80029ADC -> 0x8002D8D4 is the recovered match-session
     * owner. It configures the two 512x240 draw/display pairs, applies the
     * optional venue-name substitution, runs the initialize/load/loop/
     * teardown stages, restores the team record, clears the transition and
     * waits eleven presentations. Keep the retail routine's independent
     * custom-location recheck and repeated unchecked team-index loads: a
     * changing flag/index can skip, invent or split the two restore stores. */
    TRY(direct_call(run, 0x80029adcu, 0x8002d8d4u, 0, 0, 0, 0, &value));
    /* GAMEONLY 0x80029AE4 -> 0x80029E58 is the recovered loading-screen
     * compositor. It loads zloadscr.psh, resolves LdS1, DrawSyncs around
     * uploads at (0,0), (0,256) and (512,0), then releases the archive. Its
     * silent null-resource path and unchecked null-image dispatch remain in
     * that owner rather than being normalized by main. */
    TRY(direct_call(run, 0x80029ae4u, 0x80029e58u, 0, 0, 0, 0, &value));
    TRY(write_word(run, 0x800d7af8u, 0x80029af8u, run->s0));
    /* GAMEONLY 0x80029AFC -> 0x80029BFC is the recovered resource-loader
     * retry wrapper. It keeps calling 0x800941C8 with "feload.bin" and flags
     * zero until a nonzero pointer arrives. Persistent failure remains the
     * original tight infinite retry; the native owner only exposes a bounded
     * NBA97_TEXT_LIMIT diagnostic instead of inventing a NULL return. */
    TRY(direct_call(run, 0x80029afcu, 0x80029bfcu, 2, 0x800247ecu, 0, 0, &value));
    if (!value.known) {
        stop(run, 0x80029b04u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    run->s1 = value.word;
    out->loaded_image = run->s1;
    /* GAMEONLY 0x80029B08 -> 0x80090D60 is the recovered heap payload-size
     * wrapper. It finds the allocation descriptor through 0x80090618, then
     * returns requested-size word descriptor+0x14 for the FELOAD transfer.
     * Keep the original unchecked-null bug: descriptor zero still reads low
     * RAM address 0x14 rather than being normalized to a zero/error result. */
    TRY(direct_call(run, 0x80029b08u, 0x80090d60u, 1, run->s1, 0, 0, &value));
    if (!value.known) {
        stop(run, 0x80029b10u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    run->s2 = value.word;
    out->loaded_image_size = run->s2;
    out->loaded_feload = 1;
    run->s0 = 0;
    TRY(write_word(run, 0x800d7af8u, 0x80029b1cu, 0));
    for (i = 0; i < 20; ++i) {
        ++run->s0; /* 0x80029B24 delay slot executes before the callee. */
        /* Each source iteration crosses the recovered 0x80029BDC wrapper. */
        TRY(direct_call(run, 0x80029b20u, 0x80029bdcu, 0, 0, 0, 0, &value));
    }
    TRY(direct_call(run, 0x80029b34u, 0x8009dba0u, 2, 0, 0, 0, &value));
    TRY(direct_call(run, 0x80029b3cu, 0x8009dbe0u, 1, 0, 0, 0, &value));
    TRY(direct_call(run, 0x80029b44u, 0x8009dbf8u, 1, 0, 0, 0, &value));
    run->s0 = 0;
    for (i = 0; i < 20; ++i) {
        ++run->s0; /* 0x80029B54 delay slot executes before the callee. */
        /* Preserve the second twenty-presentation delay independently. */
        TRY(direct_call(run, 0x80029b50u, 0x80029bdcu, 0, 0, 0, 0, &value));
    }
    TRY(direct_call(run, 0x80029b64u, 0x800a44d4u, 0, 0, 0, 0, &value));
    TRY(direct_call(run, 0x80029b6cu, 0x8009167cu, 0, 0, 0, 0, &value));
    TRY(direct_call(run, 0x80029b74u, 0x8008f19cu, 0, 0, 0, 0, &value));
    TRY(direct_call(run, 0x80029b84u, 0x800a3a74u, 2, 0x800d6decu, 0x20u, 0, &value));
    TRY(direct_call(run, 0x80029b94u, 0x800aa468u, 3, run->s1,
        0x801e0000u, run->s2, &value));
    TRY(read_word(run, 0x801e0000u, 0x80029ba0u, &indirect));
    out->indirect_entry = indirect;
    TRY(invoke(run, 0x80029ba8u, indirect, NBA97_GAME_MAIN_INDIRECT_CALL,
        0, 0, 0, 0, &value, &outcome));
    if (outcome == NBA97_GAME_MAIN_CALLEE_TRANSFERRED) {
        out->transferred = 1;
        out->completed = 1;
        out->saved_register[0] = run->s0;
        out->saved_register[1] = run->s1;
        out->saved_register[2] = run->s2;
        stop(run, 0, 0, 0);
        return NBA97_TEXT_COMPLETE;
    }

    /* A returning loaded overlay resumes the live source epilogue. */
    TRY(read_word(run, run->sp + 0x24u, 0x80029bb0u,
        &out->restored_return_address));
    TRY(read_word(run, run->sp + 0x20u, 0x80029bb4u, &run->s2));
    TRY(read_word(run, run->sp + 0x1cu, 0x80029bb8u, &run->s1));
    TRY(read_word(run, run->sp + 0x18u, 0x80029bbcu, &run->s0));
    out->stack_pointer = run->sp + 0x28u;
    out->saved_register[0] = run->s0;
    out->saved_register[1] = run->s1;
    out->saved_register[2] = run->s2;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
