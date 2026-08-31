#ifndef NBA97_GAME_PAGE_OFFSET_H
#define NBA97_GAME_PAGE_OFFSET_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GamePageOffsetByte {
    uint8_t value;
    uint8_t known;
} Nba97GamePageOffsetByte;

typedef struct Nba97GamePageOffsetReadEvent {
    uint32_t pc;
    uint32_t address;
    uint8_t width;
} Nba97GamePageOffsetReadEvent;

/* Execute the reached 993DC byte read synchronously. The callback may mutate
 * retained state before it returns; a later read must sample that live state.
 * Return NBA97_GAME_PAGE_OFFSET_OK only after filling a canonical known flag.
 * The event pointer may not escape the call. */
typedef int (*Nba97GamePageOffsetRead)(void *,
    const Nba97GamePageOffsetReadEvent *,Nba97GamePageOffsetByte *);

typedef struct Nba97GamePageOffsetContext {
    Nba97GamePageOffsetRead read;
    void *user;
} Nba97GamePageOffsetContext;

typedef struct Nba97GamePageOffsetProgress {
    size_t reads;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint8_t completed;
} Nba97GamePageOffsetProgress;

enum Nba97GamePageOffsetStatus {
    NBA97_GAME_PAGE_OFFSET_OK=1,
    NBA97_GAME_PAGE_OFFSET_ARGUMENT=0,
    NBA97_GAME_PAGE_OFFSET_UNKNOWN=-1,
    NBA97_GAME_PAGE_OFFSET_READ_REQUIRED=-2
};

/* Exact visible semantics of GAME 8009BF98..8009C060, including its calls to
 * 993DC. Arguments and result are full original register words. The source
 * samples byte 800C55C0 once; unless that value is 1, it samples it again.
 * Only first==1 or second==2 selects the alternate page encoding. Private ABI
 * stack/saved-register traffic is intentionally outside this typed boundary.
 * On refusal, result is unchanged and completed remains zero. */
int nba97_game_page_offset(Nba97GamePageOffsetContext *,uint32_t texture_mode,
    uint32_t abr,uint32_t x,uint32_t y,uint32_t *result,
    Nba97GamePageOffsetProgress *);

#ifdef __cplusplus
}
#endif
#endif
