#include "frontend_resource.h"

static int valid_span(const uint8_t* data, uint32_t bytes) {
    return bytes <= INT32_MAX && (data || !bytes);
}
static uint32_t little32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
int nba97_resource_crc16(const uint8_t* data, uint32_t bytes, uint16_t* checksum) {
    uint32_t crc = 0xfbeau;
    uint32_t i;
    if (!checksum || !valid_span(data, bytes)) return 0;
    for (i = 0; i < bytes; ++i) {
        unsigned bit;
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xa001u : 0u);
    }
    *checksum = (uint16_t)crc;
    return 1;
}
int nba97_resource_validate_file(const uint8_t* data, uint32_t bytes,
    uint32_t strict, Nba97ResourceValidation* result) {
    Nba97ResourceValidation r = {0, 0, 0, 0};
    uint16_t crc;
    if (!result || !valid_span(data, bytes)) return -1;
    r.payload_bytes = bytes;
    if (bytes >= 12 && data[bytes - 12] == 'C' && data[bytes - 11] == 'R' &&
        data[bytes - 10] == 'C' && data[bytes - 9] == 'F') {
        r.trailer_present = 1;
        r.payload_bytes = bytes - 12;
        r.stored_checksum = little32(data + bytes - 4);
        nba97_resource_crc16(data, r.payload_bytes, &crc);
        r.calculated_checksum = crc;
        *result = r;
        /* Full32-bit comparison: do not truncate a malformed stored checksum.
         * Source ignores the file's trailer length word and uses D9B3C=12. */
        return r.calculated_checksum == r.stored_checksum;
    }
    *result = r;
    return strict == 0;
}
int nba97_portrait_checksum_accept(const uint8_t* data, uint32_t bytes,
    uint32_t* selection_blocked) {
    uint16_t crc;
    uint32_t stored;
    if (!selection_blocked || bytes < 2 || !valid_span(data, bytes)) return -1;
    nba97_resource_crc16(data, bytes - 2, &crc);
    stored = (uint32_t)data[bytes - 2] | ((uint32_t)data[bytes - 1] << 8);
    if (crc != stored) return 0;
    /*30EFC does not test current state24, decoding success, or visibility. */
    *selection_blocked = 0;
    return 1;
}

int nba97_cool_index_load(Nba97CoolIndexLoad* s, uint32_t* index_data,
    uint32_t index_path, uint32_t archive_path, Nba97CoolIndexInvoke call,
    void* context) {
    uint32_t voice;
    if (!s || !index_data || !call) return 0;
    if (*index_data) {
        call(context, NBA97_COOL_INDEX_FREE_DATA, *index_data, 0, 0);
        *index_data = 0;
    }
    /* Original null-path call frees the index but leaves archive_path, bank,
     * voice, and sample_data untouched. Do not modernize it into full reset. */
    if (!index_path || !archive_path) return 1;
    call(context, NBA97_COOL_INDEX_DRAIN, 0, 0, 0);
    call(context, NBA97_COOL_INDEX_REQUEST, index_path, 0, 400);
    while (!s->loaded_data)
        call(context, s->graphics ? NBA97_COOL_INDEX_PUMP_UI :
            NBA97_COOL_INDEX_PUMP_IO, 0, 0, 0);
    voice = s->voice;
    s->pending = 0;
    *index_data = s->loaded_data;
    if (!(voice & 0x80000000u) &&
        !call(context, NBA97_COOL_INDEX_VOICE_STATUS, voice, 0, 0)) {
        call(context, NBA97_COOL_INDEX_FADE, s->voice, 100, UINT32_MAX);
        while (!call(context, NBA97_COOL_INDEX_VOICE_STATUS, s->voice, 0, 0)) {
            /* Source busy-spins with NO I/O pump if graphics is zero. */
            if (s->graphics) call(context, NBA97_COOL_INDEX_PUMP_UI, 0, 0, 0);
        }
        s->voice = UINT32_MAX;
    }
    /* Original already-finished nonnegative voice retains its old value. */
    if (!(s->bank & 0x80000000u)) {
        call(context, NBA97_COOL_INDEX_UNLOAD_BANK, s->bank_context, s->bank, 0);
        s->bank = UINT32_MAX;
    }
    if (s->sample_data) {
        call(context, NBA97_COOL_INDEX_FREE_DATA, s->sample_data, 0, 0);
        s->sample_data = 0;
    }
    s->archive_path = archive_path;
    return 1;
}
