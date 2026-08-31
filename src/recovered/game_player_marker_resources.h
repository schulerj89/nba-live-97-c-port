#ifndef NBA97_GAME_PLAYER_MARKER_RESOURCES_H
#define NBA97_GAME_PLAYER_MARKER_RESOURCES_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Original4D490/4CAF4: ball, reflection, shadow and arrow resources/packets.
 * Uses the frame's exact-address, per-byte knowledge access contract. Source
 * pointers and every alias retain their original encoded addresses; no private
 * CPU stack/code may alias visible input. The owner does not allocate memory,
 * replace missing resources, initialize unread padding, or publish a frame.
 * All reached source writes survive refusal, including writes made by IO.
 */
typedef struct Nba97PlayerMarkerCall {
    uint32_t pc,entry,args[5];
} Nba97PlayerMarkerCall;
/* Required synchronous REAL callees:29BFC(filename,0),90698(resource),
 * 946B8(image,x,y,clut_x,clut_y),994F4(0). A successful29BFC must perform its
 * actual loader/retry/allocator work and return a known original pointer.
 * Upload must run the owned image CPU converter and actual backend; SYNC does
 * not clear D7B14. Descriptors/access metadata remain stable during IO, but
 * source RAM, knowledge, aliases and allocation lifetime may change. Validate
 * the next reached access against live state, never stale entry snapshots.
 * Return BODY_* statuses; a missing callback is MARKER_IO_REQUIRED. */
typedef int (*Nba97PlayerMarkerIo)(void*,const Nba97PlayerMarkerCall*,Nba97GamePeriodValue*);
typedef struct Nba97PlayerMarkerContext {
    Nba97PlayerFrameAccess access;Nba97PlayerMarkerIo io;void* user;
    size_t operation_budget;
} Nba97PlayerMarkerContext;
typedef struct Nba97PlayerMarkerProgress {
    size_t operations,reads,stores,calls,packets,arrows,copies;
    uint32_t stopped_pc,stopped_address;uint8_t completed;
} Nba97PlayerMarkerProgress;
enum {NBA97_MARKER_IO_REQUIRED=-12};
int nba97_game_player_marker_resources(Nba97PlayerMarkerContext*,Nba97PlayerMarkerProgress*);
/*4CAF4 requires already-published DCE04 and live SHPP/name bytes. */
int nba97_game_player_marker_arrows(Nba97PlayerMarkerContext*,Nba97PlayerMarkerProgress*);
/*50E74, or50F88 when reflected==1. Source UV dimensions use low bytes. */
int nba97_game_player_marker_packet(Nba97PlayerMarkerContext*,uint32_t packet,
    uint32_t image,unsigned reflected,Nba97PlayerMarkerProgress*);
/* AA468's reached aligned16/32/528-byte domains, preserving read/store groups,
 * overlap direction, partial knowledge and original signed ADD traps. Other
 * lengths/alignment explicitly refuse; this is not a replacement memcpy. */
int nba97_game_player_marker_copy(Nba97PlayerMarkerContext*,uint32_t source,
    uint32_t destination,unsigned bytes,Nba97PlayerMarkerProgress*);
#ifdef __cplusplus
}
#endif
#endif
