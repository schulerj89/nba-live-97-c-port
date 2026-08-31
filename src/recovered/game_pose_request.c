#include "game_pose_request.h"
#include <string.h>

static int hk(const Nba97GamePoseEntity* e, unsigned field) { return (e->half_known & (1u<<field))!=0; }
static int wk(const Nba97GamePoseEntity* e, unsigned field) { return (e->word_known & (1u<<field))!=0; }
static void hs(Nba97GamePoseEntity* e, unsigned field, uint16_t value) {
    e->half[field]=value; e->half_known|=1u<<field;
}
static int32_t signed_word(uint32_t value) {
    return value<=0x7fffffffu ? (int32_t)value : -1-(int32_t)~value;
}
static void ws(Nba97GamePoseEntity* e, unsigned field, uint32_t value) {
    e->word[field]=signed_word(value); e->word_known|=1u<<field;
}
static const Nba97GameMocapHeader* clip(const Nba97GameMocapIndex* index,
                                     unsigned channel, uint16_t id) {
    uint16_t ref;
    if (id>=84) return 0;
    ref=index->reference[channel][id];
    return ref<index->header_count ? &index->header[ref] : 0;
}
static Nba97GamePoseResult channel_request(const Nba97GameMocapIndex* index,
    Nba97GamePoseEntity* e, unsigned ch, const Nba97GameMocapHeader* current) {
    const unsigned id=NBA97_POSE_46+ch*2, previous=id+1;
    const unsigned logical=NBA97_POSE_50+ch*2, old_logical=logical+1;
    const unsigned request=NBA97_POSE_84+ch*2, frame=NBA97_POSE_8C+ch*2;
    uint16_t tick;
    if (!hk(e,previous)) return NBA97_GAME_POSE_UNKNOWN;
    if (e->half[previous]<0x8000u) {
        const Nba97GameMocapHeader* prior;
        hs(e,request,e->half[id]); hs(e,request+1,e->half[previous]);
        prior=clip(index,ch,e->half[previous]);
        if (!prior) return NBA97_GAME_POSE_REFERENCE;
        hs(e,NBA97_POSE_62+ch*2,prior->flags);
        if (!hk(e,logical)) return NBA97_GAME_POSE_UNKNOWN;
        tick=e->half[logical]; hs(e,frame,(uint16_t)((current->flags&8u) ? tick>>1 : tick));
        if (!hk(e,old_logical)) return NBA97_GAME_POSE_UNKNOWN;
        tick=e->half[old_logical]; hs(e,frame+1,(uint16_t)((prior->flags&8u) ? tick>>1 : tick));
    } else {
        if (!hk(e,logical)) return NBA97_GAME_POSE_UNKNOWN;
        tick=e->half[logical];
        if ((current->flags&8u) && (tick&1u)) {
            hs(e,request+1,e->half[id]); hs(e,request,e->half[id]);
            hs(e,frame,(uint16_t)(tick>>1));
            /* Source compares a widened tick+1; 65535 never wraps to zero here. */
            hs(e,frame+1,(uint16_t)((uint32_t)tick+1u==current->count ? 0u : (tick>>1)+1u));
            if (!hk(e,NBA97_POSE_9A)) return NBA97_GAME_POSE_UNKNOWN;
            hs(e,NBA97_POSE_94+ch,128);
            hs(e,NBA97_POSE_9A,(uint16_t)((e->half[NBA97_POSE_9A]&~(4u<<ch)) |
                ((e->half[NBA97_POSE_9A]&(1u<<ch))<<2)));
        } else {
            if (current->flags&8u) {
                hs(e,frame,(uint16_t)(tick>>1)); hs(e,request+1,0xffff);
            } else {
                hs(e,request+1,0xffff); hs(e,frame,tick);
            }
            hs(e,request,e->half[id]);
        }
    }
    return NBA97_GAME_POSE_OK;
}
Nba97GamePoseResult nba97_game_pose_requests(const Nba97GameMocapIndex* index,
    Nba97GamePoseEntity entities[10], Nba97GamePoseFootCallback foot,
    void* context, unsigned* completed) {
    unsigned i;
    if (!index || !entities || !completed || index->header_count>168) return NBA97_GAME_POSE_ARGUMENT;
    *completed=0;
    for (i=0;i<10;++i) {
        Nba97GamePoseEntity* e=&entities[i];
        const Nba97GameMocapHeader* current[2];
        Nba97GamePoseResult result; unsigned leg; uint16_t counter;
        Nba97GameFootOffset offset;
        if (!hk(e,NBA97_POSE_46)) return NBA97_GAME_POSE_UNKNOWN;
        current[0]=clip(index,0,e->half[NBA97_POSE_46]);
        if (!current[0]) return NBA97_GAME_POSE_REFERENCE;
        if (!hk(e,NBA97_POSE_4A)) return NBA97_GAME_POSE_UNKNOWN;
        hs(e,NBA97_POSE_60,current[0]->flags);
        current[1]=clip(index,1,e->half[NBA97_POSE_4A]);
        if (!current[1]) return NBA97_GAME_POSE_REFERENCE;
        hs(e,NBA97_POSE_64,current[1]->flags);
        result=channel_request(index,e,0,current[0]); if (result) return result;
        result=channel_request(index,e,1,current[1]); if (result) return result;
        if (!(e->half[NBA97_POSE_64]&0xc0u)) { hs(e,NBA97_POSE_EC,0); *completed=i+1; continue; }
        if (!wk(e,NBA97_POSE_10)) return NBA97_GAME_POSE_UNKNOWN;
        if (e->word[NBA97_POSE_10]!=0) { hs(e,NBA97_POSE_EC,0); *completed=i+1; continue; }
        if (!e->foot_e0_known) return NBA97_GAME_POSE_UNKNOWN;
        leg=(e->half[NBA97_POSE_64]&0x40u) ? 0u : 1u;
        if ((e->foot_e0!=0)!=(leg!=0)) hs(e,NBA97_POSE_EC,0);
        e->foot_e0=(uint8_t)leg; e->foot_e0_known=1;
        if (!foot || !foot(context,i,e,leg,&offset)) return NBA97_GAME_POSE_FOOT_REQUIRED;
        if (!hk(e,NBA97_POSE_EC)) return NBA97_GAME_POSE_UNKNOWN;
        counter=(uint16_t)(e->half[NBA97_POSE_EC]+1u); hs(e,NBA97_POSE_EC,counter);
        /* Preserve the signed counter-wrap bug: values8000..FFFF lock as <4. */
        if (counter<4u || counter>=0x8000u) {
            if (!wk(e,NBA97_POSE_08) || !wk(e,NBA97_POSE_0C)) return NBA97_GAME_POSE_UNKNOWN;
            ws(e,NBA97_POSE_30,(uint32_t)e->word[NBA97_POSE_08]+(uint32_t)offset.x);
            ws(e,NBA97_POSE_34,(uint32_t)e->word[NBA97_POSE_0C]+(uint32_t)offset.z);
        } else {
            if (!wk(e,NBA97_POSE_30) || !wk(e,NBA97_POSE_34)) return NBA97_GAME_POSE_UNKNOWN;
            ws(e,NBA97_POSE_08,(uint32_t)e->word[NBA97_POSE_30]-(uint32_t)offset.x);
            ws(e,NBA97_POSE_0C,(uint32_t)e->word[NBA97_POSE_34]-(uint32_t)offset.z);
        }
        *completed=i+1;
    }
    return NBA97_GAME_POSE_OK;
}
Nba97GamePoseResult nba97_game_pose_packet(const Nba97GamePoseEntity* e,
                                         Nba97GamePosePacket* out) {
    Nba97GamePosePacket packet; unsigned ch;
    if (!e || !out) return NBA97_GAME_POSE_ARGUMENT;
    if (!hk(e,NBA97_POSE_9A)) return NBA97_GAME_POSE_UNKNOWN;
    memset(&packet,0,sizeof packet); packet.conversion=e->half[NBA97_POSE_9A];
    for (ch=0;ch<2;++ch) {
        unsigned id=NBA97_POSE_84+ch*2, frame=NBA97_POSE_8C+ch*2;
        if (!hk(e,id) || !hk(e,id+1) || !hk(e,frame)) return NBA97_GAME_POSE_UNKNOWN;
        packet.clip[ch][0]=e->half[id]; packet.clip[ch][1]=e->half[id+1];
        packet.frame[ch][0]=e->half[frame];
        if (e->half[id+1]<0x8000u) {
            if (!hk(e,frame+1) || !hk(e,NBA97_POSE_94+ch)) return NBA97_GAME_POSE_UNKNOWN;
            packet.frame[ch][1]=e->half[frame+1]; packet.weight[ch]=e->half[NBA97_POSE_94+ch];
        }
    }
    *out=packet; return NBA97_GAME_POSE_OK;
}
