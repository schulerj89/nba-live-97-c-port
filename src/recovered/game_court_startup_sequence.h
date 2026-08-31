#ifndef NBA97_GAME_COURT_STARTUP_SEQUENCE_H
#define NBA97_GAME_COURT_STARTUP_SEQUENCE_H
#include "game_court_interactive.h"
#include "game_court_packet_startup.h"
#include "game_court_resources.h"
#include "game_court_roster_startup.h"
#include "game_court_startup.h"
#include "game_court_textures.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCourtStartupAllocation {
    uint32_t raw_address;
    Nba97GameImageReference image;
} Nba97GameCourtStartupAllocation;

typedef struct Nba97GameCourtStartupSequenceContext {
    /* Every source CPU child receives this exact retained address space. */
    Nba97GameTextMemory memory;
    const Nba97GameCourtStartupAllocation* allocation;
    size_t allocation_count;
    Nba97CourtInteractiveIo interactive_io;
    void* interactive_user;
    Nba97CourtPacketStartupIo packet_io;
    void* packet_user;
    Nba97GameCourtStartupIo startup_io;
    void* startup_user;
    Nba97GameImageTransferIo image_io;
    void* image_user;
    Nba97GameTextPoolIo pool_io;
    void* pool_user;
    Nba97GameCourtTextureState* texture_state;
} Nba97GameCourtStartupSequenceContext;

typedef struct Nba97GameCourtStartupSequenceBudgets {
    size_t roster_accesses;
    size_t interactive_accesses;
    size_t packet_accesses;
    size_t texture_select_accesses;
    size_t texture_images;
    size_t texture_headers;
    size_t geometry_select_accesses;
    size_t resource_accesses;
} Nba97GameCourtStartupSequenceBudgets;

typedef struct Nba97GameCourtStartupSequenceJournals {
    Nba97GameCourtRosterEvent* roster;
    size_t roster_capacity;
    Nba97CourtInteractiveEvent* interactive;
    size_t interactive_capacity;
    Nba97CourtPacketStartupEvent* packet;
    size_t packet_capacity;
    Nba97GameCourtStartupEvent* texture_select;
    size_t texture_select_capacity;
    Nba97GameCourtStartupEvent* geometry_select;
    size_t geometry_select_capacity;
    Nba97GameTextPoolEvent* resources;
    size_t resource_capacity;
} Nba97GameCourtStartupSequenceJournals;

enum Nba97GameCourtStartupSequenceStage {
    NBA97_COURT_SEQUENCE_NONE=0,
    NBA97_COURT_SEQUENCE_ROSTER,
    NBA97_COURT_SEQUENCE_INTERACTIVE,
    NBA97_COURT_SEQUENCE_PACKET,
    NBA97_COURT_SEQUENCE_TEXTURE_SELECT,
    NBA97_COURT_SEQUENCE_TEXTURE_RESOLVE,
    NBA97_COURT_SEQUENCE_TEXTURES,
    NBA97_COURT_SEQUENCE_GEOMETRY_SELECT,
    NBA97_COURT_SEQUENCE_RESOURCES,
    NBA97_COURT_SEQUENCE_PRIVATE_EPILOGUE,
    NBA97_COURT_SEQUENCE_COMPLETE
};

typedef struct Nba97GameCourtStartupSequenceProgress {
    Nba97GameCourtRosterProgress roster;
    Nba97CourtInteractiveProgress interactive;
    Nba97CourtPacketStartupProgress packet;
    Nba97GameCourtStartupProgress texture_select;
    Nba97GameCourtTextureProgress textures;
    Nba97GameCourtStartupProgress geometry_select;
    Nba97GameCourtResourceProgress resources;
    Nba97GameImageReference texture_reference;
    uint32_t loaded_texture,loaded_geometry;
    int child_result;
    size_t source_intervals_completed;
    uint8_t stage,completed,natural_entry;
} Nba97GameCourtStartupSequenceProgress;

/* Pure exact-address lookup. It does not inspect or mutate image payloads. A
 * duplicate matching raw address is ambiguous and is rejected. Alignment
 * provenance must agree with the source address before the image owner runs. */
int nba97_game_court_startup_resolve_image(
    const Nba97GameCourtStartupAllocation*,size_t count,uint32_t raw_address,
    Nba97GameImageReference*);

/* Canonical C99 owner for GAME479B8..48D5C. Children run in exact source
 * order, share one retained CPU memory, and keep their separately typed IO,
 * journals, budgets and progress. No later interval is preflighted: failure
 * returns that child's exact status and preserves every completed prefix.
 * The private48D28 epilogue only restores ABI stack/register state, so it has
 * no public retained-memory event. This closes a callable source body, not the
 * real52C20 producers/services; natural_entry therefore remains false. */
int nba97_game_court_startup_sequence(
    Nba97GameCourtStartupSequenceContext*,
    const Nba97GameCourtStartupSequenceBudgets*,
    const Nba97GameCourtStartupSequenceJournals*,
    Nba97GameCourtStartupSequenceProgress*);

#ifdef __cplusplus
}
#endif
#endif
