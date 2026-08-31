#include "frontend_resource_cleanup.h"

int nba97_frontend_announcer_stop(Nba97CoolIndexLoad* s,
    Nba97FrontendCleanupInvoke call, void* context) {
    int stopped = 0;
    if (!s || !call) return -1;
    call(context, NBA97_FRONTEND_CLEANUP_CANCEL, 20, 0, 0);
    if (!(s->voice & 0x80000000u) &&
        !call(context, NBA97_FRONTEND_CLEANUP_VOICE_STATUS, s->voice, 0, 0)) {
        call(context, NBA97_FRONTEND_CLEANUP_FADE, s->voice, 20, UINT32_MAX);
        /*313C8 busy-waits without a UI/I/O pump or timeout. This20-unit fade
         * differs from the100-unit fade in3122C. Preserve both owners. */
        while (!call(context, NBA97_FRONTEND_CLEANUP_VOICE_STATUS, s->voice, 0, 0)) {}
        s->voice = UINT32_MAX;
        stopped = 1;
    }
    /* A nonnegative, already-finished handle stays unchanged in the source. */
    if (!(s->bank & 0x80000000u)) {
        call(context, NBA97_FRONTEND_CLEANUP_UNLOAD_BANK, s->bank_context, s->bank, 0);
        s->bank = UINT32_MAX;
    }
    if (s->sample_data) {
        call(context, NBA97_FRONTEND_CLEANUP_FREE_DATA, s->sample_data, 0, 0);
        s->sample_data = 0;
    }
    return stopped;
}

int nba97_frontend_resource_cleanup(Nba97FrontendResourceCleanup* s,
    Nba97CoolIndexLoad* sound, uint32_t* cool_index_data, uint32_t buffer_target,
    Nba97FrontendCleanupInvoke call, void* context) {
    unsigned i;
    if (!s || !sound || !cool_index_data || !call) return 0;
    call(context, NBA97_FRONTEND_CLEANUP_DRAIN, 0, 0, 0);
    if (s->portrait_index) {
        call(context, NBA97_FRONTEND_CLEANUP_FREE_DATA, s->portrait_index, 0, 0);
        s->portrait_index = 0;
    }
    nba97_frontend_announcer_stop(sound, call, context);
    if (*cool_index_data) {
        call(context, NBA97_FRONTEND_CLEANUP_FREE_DATA, *cool_index_data, 0, 0);
        *cool_index_data = 0;
    }
    sound->pending = 0;
    call(context, NBA97_FRONTEND_CLEANUP_BUFFER_WAIT, buffer_target, 480, 0x8003282cu);
    call(context, NBA97_FRONTEND_CLEANUP_SYNC, 0, 0, 0);
    for (i = 0; i < 2; ++i) {
        if (s->portrait[i].graphic && s->portrait[i].data)
            call(context, NBA97_FRONTEND_CLEANUP_FREE_DATA, s->portrait[i].data, 0, 0);
        /* Original2FBA8 skips FREE when graphic==0, even with nonzero data,
         * then drops the data field. Do not add a speculative extra free. */
        s->portrait[i].graphic = 0;
        s->portrait[i].data = 0;
        /* physical_record is deliberately not reset by this owner. */
    }
    if (s->card_data) {
        call(context, NBA97_FRONTEND_CLEANUP_FREE_DATA, s->card_data, 0, 0);
        s->card_data = 0;
    }
    call(context, NBA97_FRONTEND_CLEANUP_SYNC, 0, 0, 0);
    call(context, NBA97_FRONTEND_CLEANUP_HARDWARE_WAIT, 0, 0, 0);
    return 1;
}
