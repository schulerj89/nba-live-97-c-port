#ifndef NBA97_GAME_COURT_PACKETS_H
#define NBA97_GAME_COURT_PACKETS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97CourtResult {
    NBA97_COURT_COMPLETE=1, NBA97_COURT_ARGUMENT=0,
    NBA97_COURT_UNKNOWN=-1, NBA97_COURT_RESOURCE=-2,
    NBA97_COURT_ALIGNMENT=-3, NBA97_COURT_LIMIT=-4,
    NBA97_COURT_MATH_REQUIRED=-5
};
typedef struct Nba97CourtValue { uint32_t word; uint8_t known; } Nba97CourtValue;
/* Synchronous retained native storage, not an emulated CPU address space.
 * Encoded addresses require actual source-allocation provenance. Resolve every
 * access again; preserve aliases. Width3 is the low24 part of an ALIGNED tag
 * word, not a read/write of its high byte. Unknown/malformed metadata, missing
 * ownership or failed writes refuse before that access. Other widths are1/2/4.
 * Read results have canonical known0/word0 or known1/masked word. Writes always
 * carry a known value and establish only those bytes. Context, backing storage
 * and metadata must stay alive; do not mutate request/progress objects. */
typedef int (*Nba97CourtAccess)(void*,uint32_t pc,uint32_t address,
                              unsigned width,int write,Nba97CourtValue*);
enum Nba97CourtMathKind {
    NBA97_COURT_PROJECT_THREE=0, NBA97_COURT_PROJECT_ONE,
    NBA97_COURT_NORMAL_CLIP, NBA97_COURT_SCREEN,
    NBA97_COURT_LEADING_BITS, NBA97_COURT_AVERAGE_FOUR,
    NBA97_COURT_ORDER_DEPTH, NBA97_COURT_LOAD_VERTEX_WORD,
    NBA97_COURT_LOAD_ROTATION_WORD, NBA97_COURT_LOAD_TRANSLATION_WORD
};
typedef struct Nba97CourtMathRequest {
    enum Nba97CourtMathKind kind;
    uint32_t pc;
    uint32_t word; /* LOAD_VERTEX_WORD: packedXY or rawZ (Z narrows to signed16). */
    unsigned index; /* SCREEN0..2; vertex0..5; rotation0..4; translation0..2. */
} Nba97CourtMathRequest;
/* Actual geometry math over the SAME retained camera/geometry state. Projection
 * consumes the retained vertex inputs and advances screen/depth FIFOs;
 * LOAD_VERTEX_WORD preserves input changes even when the quad is rejected.
 * NORMAL_CLIP and AVERAGE_FOUR retain
 * their arithmetic side effects. SCREEN, LEADING_BITS, NORMAL_CLIP and
 * ORDER_DEPTH return the actual consumed word. LEADING_BITS is data-register31
 * (LZCR), NOT control-register31 (FLAG). Never substitute projection flags.
 * Return COMPLETE only after the real operation; unavailable math must refuse.
 * A callback that merely supplies projected points is a test fixture, not a
 * production geometry backend. Callbacks may synchronously mutate owned memory.
 * No generic CPU, instruction decoder, or emulator dependency is provided. */
typedef int (*Nba97CourtMath)(void*,const Nba97CourtMathRequest*,Nba97CourtValue*);
typedef struct Nba97CourtContext {
    Nba97CourtAccess access;
    Nba97CourtMath math;
    void* user;
    size_t operation_budget; /* Native bound, never a repaired source count. */
} Nba97CourtContext;
typedef struct Nba97CourtProgress {
    size_t operations,reads,stores,math_operations,quads,linked;
    uint32_t stopped_pc,stopped_address;
    uint8_t completed;
} Nba97CourtProgress;
enum Nba97CourtPacketPass {
    NBA97_COURT_FIXED_TEXTURED_54D4C=0,
    NBA97_COURT_DEPTH_TEXTURED_54ED8,
    NBA97_COURT_FIXED_FLAT_54E50
};
/* Complete54D4C/54ED8/54E50 CPU control, loads, projected packet writes and
 * original low24 links. Geometry math is a required backend, not assumed.
 * vertices/packets/ordering_table are proven source allocation addresses; count
 * and depth_mask are raw source words. Unused depth_mask is not consumed.
 * All three original loops execute at least once even for count0/negative.
 * 54D4C reads SIX words of the NEXT vertex record before its culling branch,
 * including on the last quad. The overread and stale packet fields on rejection
 * are preserved, not padded or cleared. Packet XY order is0,1,3,2; input vertices
 * are in perimeter order. Other packet bytes are retained.
 * Refusal retains the source prefix and math state; it is not resumable or
 * atomic. Clone the WHOLE memory/math owner externally before publication.
 * Source stack/code aliases are excluded. Tag-word bases must be aligned;
 * unaligned LWL/SWL aliases are unsupported, not a claimed original CPU trap. */
int nba97_game_court_packets(Nba97CourtContext*,enum Nba97CourtPacketPass,
    uint32_t vertices,uint32_t packets,uint32_t ordering_table,uint32_t count,
    uint32_t depth_mask,Nba97CourtProgress*);
/* Complete56914 in the aligned tag-word domain; preserve both upper bytes and
 * the original packet-store THEN table-store order, including self aliases. */
int nba97_game_court_link(Nba97CourtContext*,uint32_t ordering_word,uint32_t packet,
                         Nba97CourtProgress*);
/* Complete4AC68 court pass, including55F18/55F44 camera loads, the three packet
 * builders and56914 links. Reads actual retained GAME globals and resource
 * records through access; preserves live visibility/count/bank rereads and
 * flat edge packets. It does not load resources, select/update the camera,
 * initialize geometry controls/ordering tables, or rasterize linked packets.
 * No whole match-frame or natural-entry claim is implied by this court pass. */
int nba97_game_court_frame(Nba97CourtContext*,Nba97CourtProgress*);
#ifdef __cplusplus
}
#endif
#endif
