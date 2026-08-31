#include "game_gameplay_audio.h"

#include <string.h>

typedef struct Run {
    Nba97GameplayAudioContext* context;
    Nba97GameplayAudioProgress* progress;
} Run;

#define TRY(expression) do { \
    int status_ = (expression); \
    if (status_ != NBA97_BODY_OK) return status_; \
} while (0)

static int32_t signed32(uint32_t value) {
    return value < UINT32_C(0x80000000)
        ? (int32_t)value : -1 - (int32_t)~value;
}

static uint32_t sign16(uint32_t value) {
    return (value & UINT32_C(0x8000))
        ? (value & UINT32_C(0xffff)) | UINT32_C(0xffff0000)
        : value & UINT32_C(0xffff);
}

static uint32_t sign8(uint32_t value) {
    return (value & UINT32_C(0x80))
        ? (value & UINT32_C(0xff)) | UINT32_C(0xffffff00)
        : value & UINT32_C(0xff);
}

static uint32_t arithmetic_shift(uint32_t value, unsigned count) {
    return (value & UINT32_C(0x80000000))
        ? (value >> count) | (~UINT32_C(0) << (32 - count))
        : value >> count;
}

static unsigned known_bytes(unsigned width) {
    return (1u << width) - 1u;
}

static uint32_t width_mask(unsigned width) {
    return width == 4 ? UINT32_MAX : (UINT32_C(1) << (width * 8)) - 1u;
}

static int reserve(Run* run, uint32_t pc, uint32_t address) {
    run->progress->stopped_pc = pc;
    run->progress->stopped_address = address;
    if (run->progress->operations >= run->context->operation_budget)
        return NBA97_BODY_JOURNAL_LIMIT;
    ++run->progress->operations;
    return NBA97_BODY_OK;
}

static int access_value(Run* run, uint32_t pc, uint32_t address,
                        unsigned width, unsigned kind,
                        Nba97PlayerFrameValue* value) {
    int status;
    unsigned i;
    TRY(reserve(run, pc, address));
    if ((width == 4 && (address & 3u)) ||
        (width == 2 && (address & 1u)))
        return NBA97_BODY_ALIGNMENT_TRAP;
    status = run->context->access(run->context->user, pc, address, width,
                                  kind, value);
    if (status != NBA97_BODY_OK) return status;
    if (value->is_reference > 1 || value->reference.known > 1 ||
        (!value->reference.known &&
         (value->reference.allocation || value->reference.offset)) ||
        (!value->is_reference && value->reference.known) ||
        (value->is_reference &&
         (width != 4 || (!value->reference.known &&
                         (value->known_mask || value->word)))))
        return NBA97_BODY_ARGUMENT;
    if ((value->known_mask & ~known_bytes(width)) ||
        (value->word & ~width_mask(width)))
        return NBA97_BODY_ARGUMENT;
    for (i = 0; i < width; ++i) {
        if (!(value->known_mask & (1u << i)) &&
            (value->word & (UINT32_C(255) << (i * 8))))
            return NBA97_BODY_ARGUMENT;
    }
    if (kind == NBA97_FRAME_READ) ++run->progress->reads;
    else ++run->progress->stores;
    return NBA97_BODY_OK;
}

static int read_value(Run* run, uint32_t pc, uint32_t address,
                      unsigned width, uint32_t* word) {
    Nba97PlayerFrameValue value;
    memset(&value, 0, sizeof value);
    TRY(access_value(run, pc, address, width, NBA97_FRAME_READ, &value));
    if (value.is_reference || value.known_mask != known_bytes(width))
        return NBA97_BODY_UNKNOWN;
    *word = value.word;
    return NBA97_BODY_OK;
}

static int write_value(Run* run, uint32_t pc, uint32_t address,
                       unsigned width, uint32_t word) {
    Nba97PlayerFrameValue value;
    memset(&value, 0, sizeof value);
    value.word = word & width_mask(width);
    value.known_mask = (uint8_t)known_bytes(width);
    return access_value(run, pc, address, width, NBA97_FRAME_WRITE, &value);
}

static int service(Run* run, uint32_t pc, uint32_t entry,
                   unsigned count, uint32_t a0, uint32_t a1, uint32_t a2,
                   unsigned return_bytes, uint32_t* result) {
    Nba97GameplayAudioCall call;
    Nba97PlayerFrameValue value;
    int status;
    unsigned i;
    TRY(reserve(run, pc, entry));
    if (!run->context->service)
        return NBA97_GAMEPLAY_AUDIO_SERVICE_REQUIRED;
    memset(&call, 0, sizeof call);
    memset(&value, 0, sizeof value);
    call.pc = pc;
    call.entry = entry;
    call.argument[0] = a0;
    call.argument[1] = a1;
    call.argument[2] = a2;
    call.count = count;
    call.return_bytes = return_bytes;
    status = run->context->service(run->context->user, &call, &value);
    if (status != NBA97_BODY_OK) return status;
    ++run->progress->services;
    if (entry == UINT32_C(0x800ac080)) ++run->progress->program_calls;
    if (entry == UINT32_C(0x80093734)) ++run->progress->scheduler_calls;
    if (!return_bytes) return NBA97_BODY_OK;
    if (value.is_reference || value.reference.known ||
        value.reference.allocation || value.reference.offset ||
        value.known_mask != 15u)
        return NBA97_BODY_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        if (!(value.known_mask & (1u << i)) &&
            (value.word & (UINT32_C(255) << (i * 8))))
            return NBA97_BODY_ARGUMENT;
    }
    *result = value.word;
    return NBA97_BODY_OK;
}

/* Exact 29200 approximation. It looks like an absolute magnitude helper, but
 * negating -32768 and then sign-extending the low half restores -32768. Keep
 * that source quirk instead of replacing it with abs()/hypot(). */
static uint32_t magnitude_29200(uint32_t first, uint32_t second) {
    uint32_t a = first;
    uint32_t b = second;
    uint32_t shifted;
    uint32_t av;
    uint32_t bv;
    if (signed32(first << 16) < 0) a = 0u - first;
    shifted = second << 16;
    if (signed32(shifted) < 0) {
        b = 0u - second;
        shifted = b << 16;
    }
    bv = arithmetic_shift(shifted, 16);
    av = arithmetic_shift(a << 16, 16);
    if (signed32(bv) < signed32(av))
        return arithmetic_shift(shifted, 18) + av;
    return arithmetic_shift(a << 16, 18) + bv;
}

static uint32_t multiply_high_signed(uint32_t left, uint32_t right) {
    int64_t product = (int64_t)signed32(left) * (int64_t)signed32(right);
    return (uint32_t)((uint64_t)product >> 32);
}

static uint32_t event_zero_level(uint32_t source) {
    uint32_t original = source << 16;
    uint32_t value = sign16(source);
    uint32_t high;
    if (signed32(value) < 0) value = 0u - value;
    high = multiply_high_signed(value, UINT32_C(0x66666667));
    return arithmetic_shift(high, 1) - arithmetic_shift(original, 31);
}

static uint32_t event_one_level(uint32_t source) {
    uint32_t value = sign16(source);
    uint32_t high;
    if (signed32(value) < 0) value = 0u - value;
    value <<= 7;
    high = multiply_high_signed(value, UINT32_C(0x2aaaaaab));
    return arithmetic_shift(high, 5) - arithmetic_shift(value, 31);
}

static uint32_t vector_level(uint32_t x, uint32_t y, uint32_t z) {
    uint32_t value = sign16(magnitude_29200(sign16(x), sign16(y)));
    uint32_t shifted;
    uint32_t high;
    value = sign16(magnitude_29200(value, sign16(z)));
    shifted = value << 16;
    if (signed32(value) < 0) value = 0u - value;
    high = multiply_high_signed(value, UINT32_C(0x2aaaaaab));
    return high - arithmetic_shift(shifted, 31);
}

static int sound_29258(Run* run, uint32_t request, uint32_t* result) {
    uint32_t event = sign16(request);
    uint32_t pointer;
    uint32_t value;
    uint32_t y;
    uint32_t level;
    uint32_t program = request;
    uint32_t scaled;

    if (signed32(event) < 32) {
        TRY(read_value(run, UINT32_C(0x8002927c), UINT32_C(0x800f9ffe),
                       2, &value));
        if (!value) {
            TRY(read_value(run, UINT32_C(0x80029294),
                           UINT32_C(0x800fe860), 4, &value));
            value |= UINT32_C(1) << (event & 31u);
            TRY(write_value(run, UINT32_C(0x800292a4),
                            UINT32_C(0x800fe860), 4, value));
        }
    }

    switch (event) {
    case 0:
        TRY(read_value(run, UINT32_C(0x80029398),
                       UINT32_C(0x800fdc48), 4, &pointer));
        TRY(read_value(run, UINT32_C(0x800293a0), pointer + 0x18u, 2, &value));
        level = event_zero_level(value);
        if (signed32(level) >= 129) level = 128;
        else if (signed32(level) < 32) level = 32;
        break;
    case 1:
        TRY(read_value(run, UINT32_C(0x80029408),
                       UINT32_C(0x800fdc48), 4, &pointer));
        TRY(read_value(run, UINT32_C(0x80029410), pointer + 0x14u, 2, &value));
        level = event_one_level(value);
        if (signed32(level) >= 129) level = 128;
        else if (signed32(level) < 64) level = 64;
        break;
    case 2:
    case 3:
        TRY(read_value(run, UINT32_C(0x80029320),
                       UINT32_C(0x800fdc48), 4, &pointer));
        TRY(read_value(run, UINT32_C(0x80029328), pointer + 0x14u, 2, &value));
        TRY(read_value(run, UINT32_C(0x8002932c), pointer + 0x16u, 2, &y));
        /* The first 29200 call may mutate shared state in the source ABI, so
         * 29338 reloads FDC48 before fetching Z. Keep that visible reread. */
        TRY(read_value(run, UINT32_C(0x8002933c),
                       UINT32_C(0x800fdc48), 4, &pointer));
        TRY(read_value(run, UINT32_C(0x80029344), pointer + 0x18u, 2, &pointer));
        level = vector_level(value, y, pointer);
        if (signed32(level) >= 129) level = 128;
        else if (signed32(level) < 32) level = 32;
        break;
    case 0x60:
        program = 2;
        level = 128;
        break;
    case 0x61:
        program = 4;
        level = 128;
        break;
    case 0x62:
        program = 6;
        level = 128;
        break;
    case 0x63:
        program = 0;
        level = 128;
        break;
    default:
        level = 128;
        break;
    }

    TRY(read_value(run, UINT32_C(0x80029494), UINT32_C(0x80021d7e),
                   1, &value));
    scaled = level * value;
    if (signed32(scaled) < 0) scaled += 127;
    scaled = arithmetic_shift(scaled, 7);
    scaled *= 12;
    if (signed32(scaled) >= 128) scaled = 127;
    TRY(read_value(run, UINT32_C(0x800294a4), UINT32_C(0x80021d6c),
                   4, &value));
    return service(run, UINT32_C(0x800294dc), UINT32_C(0x800ac080), 3,
                   value, sign16(program), scaled, 4, result);
}

static int lock_enter_93d94(Run* run) {
    uint32_t value;
    TRY(read_value(run, UINT32_C(0x80093da4), UINT32_C(0x800c4b0c),
                   4, &value));
    TRY(write_value(run, UINT32_C(0x80093db4), UINT32_C(0x800c4b0c),
                    4, value + 1u));
    return read_value(run, UINT32_C(0x80093dbc), UINT32_C(0x800c4b0c),
                      4, &value);
}

static int lock_exit_93dd4(Run* run) {
    uint32_t value;
    TRY(read_value(run, UINT32_C(0x80093de8), UINT32_C(0x800c4b0c),
                   4, &value));
    TRY(write_value(run, UINT32_C(0x80093df8), UINT32_C(0x800c4b0c),
                    4, value - 1u));
    TRY(read_value(run, UINT32_C(0x80093e00), UINT32_C(0x800c4b0c),
                   4, &value));
    TRY(read_value(run, UINT32_C(0x80093e08), UINT32_C(0x800c4b0c),
                   4, &value));
    if (value) return NBA97_BODY_OK;
    for (;;) {
        TRY(read_value(run, UINT32_C(0x80093e1c), UINT32_C(0x800c4b08),
                       4, &value));
        if (!value) return NBA97_BODY_OK;
        TRY(read_value(run, UINT32_C(0x80093e38), UINT32_C(0x800c4b08),
                       4, &value));
        TRY(write_value(run, UINT32_C(0x80093e48), UINT32_C(0x800c4b08),
                        4, value - 1u));
        TRY(read_value(run, UINT32_C(0x80093e50), UINT32_C(0x800c4b08),
                       4, &value));
        TRY(service(run, UINT32_C(0x80093e54), UINT32_C(0x80093734),
                    0, 0, 0, 0, 0, 0));
    }
}

static int sequence_ab0b8(Run* run, uint32_t request, uint32_t* result) {
    uint32_t value;
    uint32_t mode;
    uint32_t base;
    uint32_t record;
    uint32_t end;
    uint32_t current;
    uint32_t program;
    uint32_t ignored;

    TRY(read_value(run, UINT32_C(0x800ab0bc), UINT32_C(0x800d7b89),
                   1, &value));
    if (!value) { *result = UINT32_C(0xfffffff6); return NBA97_BODY_OK; }
    TRY(read_value(run, UINT32_C(0x800ab0e4), UINT32_C(0x800d793c),
                   1, &mode));
    if (mode != 2) { *result = UINT32_MAX; return NBA97_BODY_OK; }
    if (request >= 16) { *result = UINT32_C(0xfffffff8); return NBA97_BODY_OK; }

    /* Read-only program context proves GAME gp=800D79C8, so gp+29C is the
     * live D7C64 sequence-table pointer. It is not a host/static pointer. */
    TRY(read_value(run, UINT32_C(0x800ab100), UINT32_C(0x800d7c64),
                   4, &base));
    record = base + 0x28u + (request << 4);
    TRY(read_value(run, UINT32_C(0x800ab10c), record, 1, &value));
    if (!sign8(value)) {
        *result = UINT32_C(0xfffffff8);
        return NBA97_BODY_OK;
    }
    TRY(lock_enter_93d94(run));
    TRY(read_value(run, UINT32_C(0x800ab12c), record + 6u, 2, &value));
    end = value << 16;
    TRY(read_value(run, UINT32_C(0x800ab134), UINT32_C(0x800d7948),
                   4, &current));
    if (signed32(current) < signed32(end)) {
        TRY(read_value(run, UINT32_C(0x800ab148), record + 4u, 2, &value));
        value = current + (value << 16);
        TRY(write_value(run, UINT32_C(0x800ab15c), UINT32_C(0x800d794c),
                        4, value));
        if (signed32(end) < signed32(value))
            TRY(write_value(run, UINT32_C(0x800ab170),
                            UINT32_C(0x800d794c), 4, end));
        TRY(read_value(run, UINT32_C(0x800ab174), record + 8u, 4, &value));
        TRY(write_value(run, UINT32_C(0x800ab17c), UINT32_C(0x800d7940),
                        1, 0));
        TRY(write_value(run, UINT32_C(0x800ab184), UINT32_C(0x800d793f),
                        1, request));
        TRY(write_value(run, UINT32_C(0x800ab194), UINT32_C(0x800d7950),
                        4, value * 3u));
    }
    TRY(read_value(run, UINT32_C(0x800ab198), record + 1u, 1, &program));
    program = sign8(program);
    if (signed32(program) >= 0) {
        uint32_t bank;
        uint32_t volume;
        TRY(read_value(run, UINT32_C(0x800ab1ac), UINT32_C(0x800d793e),
                       1, &bank));
        TRY(read_value(run, UINT32_C(0x800ab1b4), UINT32_C(0x800d793d),
                       1, &volume));
        TRY(service(run, UINT32_C(0x800ab1b8), UINT32_C(0x800ac080),
                    3, sign8(bank), program, sign8(volume), 0, &ignored));
    }
    TRY(lock_exit_93dd4(run));
    *result = 0;
    return NBA97_BODY_OK;
}

static int event_29590(Run* run, uint32_t request, uint32_t* result) {
    uint32_t enabled;
    TRY(read_value(run, UINT32_C(0x80029594), UINT32_C(0x80021d7f),
                   1, &enabled));
    if (!enabled) {
        *result = 0;
        return NBA97_BODY_OK;
    }
    return sequence_ab0b8(run, sign16(request), result);
}

int nba97_game_gameplay_audio(Nba97GameplayAudioContext* context,
                              Nba97GameplayAudioEntry entry,
                              uint32_t request,
                              Nba97GameplayAudioResult* result,
                              Nba97GameplayAudioProgress* progress) {
    Run run;
    int status;
    uint32_t word = 0;
    if (!progress || !result) return NBA97_BODY_ARGUMENT;
    memset(progress, 0, sizeof *progress);
    memset(result, 0, sizeof *result);
    if (!context || !context->access) return NBA97_BODY_ARGUMENT;
    run.context = context;
    run.progress = progress;
    if (entry == NBA97_GAMEPLAY_AUDIO_SOUND_29258)
        status = sound_29258(&run, request, &word);
    else if (entry == NBA97_GAMEPLAY_AUDIO_EVENT_29590)
        status = event_29590(&run, request, &word);
    else
        return NBA97_BODY_ARGUMENT;
    if (status == NBA97_BODY_OK) {
        result->word = word;
        result->known = 1;
        progress->completed = 1;
        progress->stopped_pc = 0;
        progress->stopped_address = 0;
    }
    return status;
}
