#include "gameplay_mocap.h"
#include <string.h>

static uint16_t read16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t read32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int extent(size_t size, uint32_t at, size_t count) {
    return at <= size && count <= size - at;
}
static int overlap(uint32_t a, uint32_t ac, uint32_t b, uint32_t bc) {
    return a <= b ? (uint64_t)b - a < ac : (uint64_t)a - b < bc;
}

Nba97GameMocapResult nba97_game_mocap_index(const uint8_t* file, size_t file_size,
                                         Nba97GameMocapIndex* out) {
    Nba97GameMocapIndex result;
    uintptr_t input_address, output_address;
    unsigned channel, slot;
    if (!file || !out) return NBA97_GAME_MOCAP_ARGUMENT;
    if (file_size < 8 || file_size > UINT32_MAX) return NBA97_GAME_MOCAP_FILE_SIZE;
    input_address = (uintptr_t)file;
    output_address = (uintptr_t)out;
    if (input_address <= output_address
            ? output_address - input_address < file_size
            : input_address - output_address < sizeof(*out))
        return NBA97_GAME_MOCAP_ARGUMENT;

    memset(&result, 0, sizeof(result));
    for (channel = 0; channel < NBA97_GAME_MOCAP_CHANNELS; ++channel) {
        uint32_t at = read32(file + channel * 4);
        if ((at & 3) || !extent(file_size, at, NBA97_GAME_MOCAP_SLOTS * 4))
            return NBA97_GAME_MOCAP_DIRECTORY;
        result.directory_offset[channel] = at;
    }
    for (channel = 0; channel < NBA97_GAME_MOCAP_CHANNELS; ++channel) {
        for (slot = 0; slot < NBA97_GAME_MOCAP_SLOTS; ++slot) {
            uint32_t at = read32(file + result.directory_offset[channel] + slot * 4);
            uint16_t index;
            Nba97GameMocapHeader* header;
            uint32_t raw_reference;
            int64_t displacement, target;
            result.reference[channel][slot] = NBA97_GAME_MOCAP_NONE;
            if (!at) continue;
            if ((at & 3) || !extent(file_size, at, 12)) return NBA97_GAME_MOCAP_HEADER;
            if (overlap(at, 12, 0, 8) ||
                overlap(at, 12, result.directory_offset[0], NBA97_GAME_MOCAP_SLOTS * 4) ||
                overlap(at, 12, result.directory_offset[1], NBA97_GAME_MOCAP_SLOTS * 4))
                return NBA97_GAME_MOCAP_OVERLAP;
            for (index = 0; index < result.header_count; ++index) {
                uint32_t previous = result.header[index].header_offset;
                if (previous == at) break;
                if (overlap(at, 12, previous, 12)) return NBA97_GAME_MOCAP_OVERLAP;
            }
            result.reference[channel][slot] = index;
            if (index < result.header_count) continue;
            /* At most168 unique headers: one per entry, with no extra insertion. */
            header = &result.header[result.header_count++];
            header->header_offset = at;
            header->source_flags = read16(file + at);
            if (header->source_flags & 0x20) return NBA97_GAME_MOCAP_RELOCATED_INPUT;
            raw_reference = read32(file + at + 8);
            /* Avoid implementation-defined uint32_t -> int32_t conversion. */
            displacement = raw_reference;
            if (raw_reference & UINT32_C(0x80000000)) displacement -= INT64_C(0x100000000);
            target = (int64_t)at + displacement;
            if (target < 0 || (uint64_t)target >= file_size) return NBA97_GAME_MOCAP_DATA_TARGET;
            header->data_offset = (uint32_t)target;
            header->flags = (uint16_t)(header->source_flags | 0x20);
            header->source_timing = header->timing = file[at + 3];
            header->source_count = header->count = file[at + 7];
            if ((header->flags & 8) && !(header->flags & 0x10)) {
                header->flags = (uint16_t)(header->flags | 0x10);
                header->timing = (uint8_t)(header->timing >> 1);
                /* Retail byte-store quirks are intentional: odd count0 ->255,
                 * even count128 ->0. Do not repair them with saturation. */
                header->count = (uint8_t)(2 * (int)header->count - ((header->flags & 1) ? 1 : 0));
            }
        }
    }
    memcpy(out, &result, sizeof(result));
    return NBA97_GAME_MOCAP_OK;
}
