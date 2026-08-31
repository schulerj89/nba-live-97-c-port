#ifndef NBA97_GAME_COURT_PACKET_STARTUP_H
#define NBA97_GAME_COURT_PACKET_STARTUP_H
#include "game_text_objects.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97CourtPacketStartupValue {uint32_t word;uint8_t known;} Nba97CourtPacketStartupValue;
typedef struct Nba97CourtPacketStartupEvent {
    uint32_t pc,address,value,entry,argument[4];
    Nba97CourtPacketStartupValue returned;
    uint8_t kind,width,argument_count,completed;
} Nba97CourtPacketStartupEvent;
enum Nba97CourtPacketStartupEventKind {
    NBA97_COURT_PACKET_STARTUP_SYNC=1,
    NBA97_COURT_PACKET_STARTUP_PAGE
};
/* Actual synchronous 994F4(0) and 9BF98(2,0,0x200,0x100) boundaries.
 * Return1 only after the requested source operation completes. PAGE must
 * return the actual known 9BF98 value; SYNC's return is unused. Calls may
 * mutate retained memory synchronously, so all later source reloads stay live.
 * Event pointers may not escape and retained metadata/lifetimes stay fixed.
 * Refusal preserves all earlier CPU stores and completed calls. */
typedef int (*Nba97CourtPacketStartupIo)(void*,const Nba97GameTextMemory*,
    const Nba97CourtPacketStartupEvent*,Nba97CourtPacketStartupValue*);
typedef struct Nba97CourtPacketStartupContext {
    Nba97GameTextMemory memory;
    size_t access_budget;
    Nba97CourtPacketStartupIo io;
    void* user;
} Nba97CourtPacketStartupContext;
typedef struct Nba97CourtPacketStartupProgress {
    size_t accesses,events,stores,services_completed,players_selected;
    size_t body_groups_scanned,body_packets_patched,bc4_packets_patched;
    uint32_t stopped_pc,stopped_address;
    uint8_t completed;
} Nba97CourtPacketStartupProgress;
/* Complete GAME479B8 interval484B8..48744 (163 source words), ending before
 * the independently owned48744 bridge. It performs real sync, scans all ten
 * scratch context-mask bits, publishes each selected F0ED8+BCC*i to F0ED4,
 * patches twenty live B0+94*n packet groups and the live BC4 packet group,
 * and writes the original global startup values. The body count is reread
 * through its descriptor after every9BF98; the BC4 route rereads F0ED4 and
 * BC4 after every call. Source/destination packet bases remain the values
 * captured before each packet loop. The body and BC4 V bytes deliberately
 * start at different values (6F and6E). Packet fields and counts are not
 * clamped or repaired; signed nonpositive counts skip their loops.
 *
 * Existing text-memory raw binding applies: original address provenance,
 * fixed disjoint source regions, possible native backing aliases, canonical
 * reached knownness, and no fabricated contexts, descriptors or packets.
 * Context/progress/journal and private ABI stack/code cannot alias visible
 * memory. Execution is bounded, prefix-preserving and not resumable. This is
 * not natural startup, a connected court frame, first possession or gameplay.
 */
int nba97_game_court_packet_startup(Nba97CourtPacketStartupContext*,
    Nba97CourtPacketStartupEvent*,size_t capacity,Nba97CourtPacketStartupProgress*);
#ifdef __cplusplus
}
#endif
#endif
