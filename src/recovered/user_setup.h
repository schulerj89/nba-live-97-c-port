#ifndef NBA97_USER_SETUP_H
#define NBA97_USER_SETUP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { NBA97_USER_CONTROLLERS=8, NBA97_USER_PROFILES=20, NBA97_USER_NAME=14 };
/* Fixed runtime slots; C++ owns any durable-ID mapping and persistence. */
typedef struct Nba97UserNames { char name[20][14]; } Nba97UserNames;
typedef struct Nba97UserRepeat {
    uint16_t last;
    int32_t clock, remaining;
} Nba97UserRepeat;
typedef struct Nba97UserSetup {
    uint8_t side[8];              /* 0 away,1 neutral,2 home */
    uint8_t assignment[8];        /* accepted:0 neutral,1 home,2 away */
    int8_t profile[8];            /* -2 none,-1 new,0..19 saved */
    int8_t alphabet[8];           /* -1 not editing */
    uint8_t cursor[8], existing[8], hide_marker[8];
    char draft[8][14];
    uint8_t start_latch, sound, controller, cancel_origin;
    int8_t result;                /* 0 running,-1 previous,6 confirm */
} Nba97UserSetup;
typedef enum Nba97UserEvent {
    NBA97_USER_NONE, NBA97_USER_SIDE, NBA97_USER_PROFILE, NBA97_USER_HELP,
    NBA97_USER_CAPACITY, NBA97_USER_EDIT_REQUEST, NBA97_USER_DELETE_REQUEST,
    NBA97_USER_REFUSED, NBA97_USER_CANCELLED, NBA97_USER_CONFIRMED,
    NBA97_USER_EDITOR_UPDATE, NBA97_USER_PROFILE_FULL, NBA97_USER_NAME_DUPLICATE,
    NBA97_USER_SAVE_REQUEST, NBA97_USER_SAVED, NBA97_USER_DELETED
} Nba97UserEvent;

/* 80037010 constructor: malformed native inputs are rejected atomically. */
int nba97_user_setup_open(Nba97UserSetup*,const uint8_t assignment[8],const int8_t profile[8]);
unsigned nba97_user_setup_row_count(unsigned topology);
int nba97_user_setup_physical(unsigned topology,unsigned row);
uint8_t nba97_user_setup_topology_mask(unsigned topology);
/* 36CA0 global gate runs BEFORE the timed disconnect/controller pass. */
Nba97UserEvent nba97_user_setup_global(Nba97UserSetup*,const uint16_t masks[8],uint8_t connected);
/* Call after global gate, at the original timed controller-update boundary. */
int nba97_user_setup_connections(Nba97UserSetup*,uint8_t connected,unsigned topology);
/* The owner visits visible rows in order, clearing each disconnected row when
 * reached, and only afterward clears topology-excluded slots. Bulk cleanup
 * above is a convenience for snapshots, never the pre-input host pass. */
int nba97_user_setup_disconnect(Nba97UserSetup*,unsigned controller);
/* A single exact token after36B80 repeat filtering; no implicit raw-mask priority.
 * Edit/delete return explicit requests until their owners complete the operation. */
Nba97UserEvent nba97_user_setup_input(Nba97UserSetup*,unsigned controller,uint16_t token,
                                    const Nba97UserNames*);
int nba97_user_setup_busy(const Nba97UserSetup*);
/* 36B80: changed mask immediately; same mask60 then12 clock units. */
uint16_t nba97_user_setup_repeat(Nba97UserRepeat*,uint16_t mask,int32_t clock);
typedef int (*Nba97UserTextWidth)(void* context,const char* text);
/* Inline37010 editor. Private alphabet bytes and font0 measurement are supplied
 * by the C++ asset owner. SAVE_REQUEST leaves the editor active until the host
 * successfully applies its transaction; no persistence belongs in this module. */
Nba97UserEvent nba97_user_setup_edit_begin(Nba97UserSetup*,unsigned controller,
                                         const Nba97UserNames*);
Nba97UserEvent nba97_user_setup_edit_input(Nba97UserSetup*,unsigned controller,uint16_t token,
                                         const Nba97UserNames*,const char alphabet[68],
                                         Nba97UserTextWidth,void* context);
Nba97UserEvent nba97_user_setup_edit_accept(Nba97UserSetup*,unsigned controller);
/* Call after accepted deletion has cleared the selected catalogue slot. */
Nba97UserEvent nba97_user_setup_deleted(Nba97UserSetup*,unsigned controller,
                                      const Nba97UserNames*);
#ifdef __cplusplus
}
#endif
#endif
