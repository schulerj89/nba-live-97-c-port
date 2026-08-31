#include "game_pose_sample.h"
#include <string.h>

static uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static int32_t s16(uint16_t v) { return v < 0x8000u ? (int32_t)v : (int32_t)v - 65536; }
static int32_t s32(uint32_t v) { return v <= 0x7fffffffu ? (int32_t)v : -1 - (int32_t)(~v); }
static int32_t asr(uint32_t v, unsigned n) {
    return s32((v >> n) | ((v & 0x80000000u) ? (~0u << (32u-n)) : 0u));
}
static int16_t half(int32_t v) { return (int16_t)s16((uint16_t)(uint32_t)v); }
static int32_t magnitude(int32_t v) { return v < 0 ? -v : v; }
static int32_t lerp(int32_t a, int32_t b, uint16_t weight) {
    uint32_t product = (uint32_t)(b-a) * weight;
    return s32((uint32_t)a + (uint32_t)asr(product,8));
}
Nba97GameEuler nba97_game_euler_blend(Nba97GameEuler a, Nba97GameEuler b,
                                    uint16_t weight) {
    int32_t ax=a.x, ay=a.y, az=a.z, dx=(int32_t)b.x-ax;
    int32_t fx, fy, fz, direct_x, direct_y, direct_z;
    int32_t shift_x, shift_y, shift_z, flip_x, flip_y, flip_z, v;
    Nba97GameEuler result;
    /* 5505C..5512C: preserve its asymmetric X search, even for noncanonical
     * signed halfword angles. Replacing this with modulo gives different values. */
    if (dx>0) {
        fx=magnitude(dx-0x800); flip_x=0x800;
        if (dx-0x800>0) { direct_x=magnitude(dx-0x1800); shift_x=0x1000; }
        else { direct_x=magnitude(dx); shift_x=0; }
    } else {
        fx=magnitude(dx+0x800); flip_x=-0x800;
        if (dx+0x800>0) { direct_x=magnitude(dx); shift_x=0; }
        else {
            direct_x=magnitude(dx+0x1000); shift_x=-0x1000;
            v=magnitude(dx-0x1000);
            if (direct_x>v) { direct_x=v; shift_x=0x1000; }
        }
    }
    v=(int32_t)b.y+ay;
    fy=magnitude(v+0x800); flip_y=-0x800-ay;
    if (v>0) { flip_y=0x800-ay; fy=magnitude((int32_t)b.y-flip_y); }
    if (fy>magnitude(v-0x1800)) { fy=magnitude(v-0x1800); flip_y=0x1800-ay; }
    v=(int32_t)b.y-ay;
    direct_y=magnitude(v+0x1000); shift_y=-0x1000;
    if (direct_y>magnitude(v)) { direct_y=magnitude(v); shift_y=0; }
    if (direct_y>magnitude(v-0x1000)) { direct_y=magnitude(v-0x1000); shift_y=0x1000; }
    v=(int32_t)b.z-az;
    fz=magnitude(v+0x800); flip_z=-0x800;
    direct_z=magnitude(v+0x1000); shift_z=-0x1000;
    if (direct_z>magnitude(v)) { direct_z=magnitude(v); shift_z=0; }
    if (fz>magnitude(v-0x800)) { fz=magnitude(v-0x800); flip_z=0x800; }
    if (direct_z>magnitude(v-0x1000)) { direct_z=magnitude(v-0x1000); shift_z=0x1000; }
    if (direct_x+direct_y+direct_z>fx+fy+fz) { ax+=flip_x; ay=flip_y; az+=flip_z; }
    else { ax+=shift_x; ay+=shift_y; az+=shift_z; }
    if (weight==128) {
        result.x=half(asr((uint32_t)(ax+b.x),1));
        result.y=half(asr((uint32_t)(ay+b.y),1));
        result.z=half(asr((uint32_t)(az+b.z),1));
    } else {
        result.x=half(lerp(ax,b.x,weight)); result.y=half(lerp(ay,b.y,weight));
        result.z=half(lerp(az,b.z,weight));
    }
    return result;
}
static const Nba97GameMocapHeader* header(const Nba97GameMocapIndex* index,
                                        unsigned channel, unsigned slot) {
    uint16_t ref;
    if (slot>=84 || index->header_count>168) return 0;
    ref=index->reference[channel][slot];
    return ref<index->header_count ? &index->header[ref] : 0;
}
static Nba97GamePoseResult frame(const uint8_t* bytes, size_t size,
    const Nba97GameMocapIndex* index, unsigned channel, uint16_t slot,
    uint16_t physical, unsigned convert, Nba97GameEuler* out, int16_t* height) {
    static const uint8_t maps[2][12]={{0,1,2,3,8,9,10,11,4,5,6,7},{4,5,6,7,0,1,2,3}};
    const Nba97GameMocapHeader* h=header(index,channel,slot);
    size_t stride=channel ? 68u : 96u, count=channel ? 8u : 12u, i;
    uint64_t at;
    const uint8_t* data;
    if (!h) return NBA97_GAME_POSE_REFERENCE;
    at=(uint64_t)h->data_offset + (uint64_t)physical*stride;
    if (at>size || stride>size-at) return NBA97_GAME_POSE_EXTENT;
    data=bytes+(size_t)at;
    if (channel) { *height=half(s16(u16(data+2))); data+=4; }
    for (i=0;i<count;++i) {
        Nba97GameEuler e;
        e.x=half(s16(u16(data+i*8))); e.y=half(s16(u16(data+i*8+2)));
        e.z=half(s16(u16(data+i*8+4)));
        if (convert) { e.x=half(0x800-e.x); e.z=half(0x800-e.z); }
        out[convert ? maps[channel][i] : i]=e;
    }
    return NBA97_GAME_POSE_OK;
}
Nba97GamePoseResult nba97_game_pose_sample(const uint8_t* bytes, size_t size,
    const Nba97GameMocapIndex* index, const Nba97GamePosePacket* packet,
    Nba97GamePose* out) {
    Nba97GamePose pose;
    unsigned channel, i;
    if (!bytes || !index || !packet || !out) return NBA97_GAME_POSE_ARGUMENT;
    memset(&pose,0,sizeof pose);
    for (channel=0;channel<2;++channel) {
        Nba97GameEuler a[12], b[12]; int16_t ah=0,bh=0;
        unsigned count=channel ? 8u : 12u, base=channel ? 0u : 8u;
        Nba97GamePoseResult status=frame(bytes,size,index,channel,packet->clip[channel][0],
            packet->frame[channel][0],packet->conversion & (1u<<channel),a,&ah);
        if (status) return status;
        if (packet->clip[channel][1]<0x8000u) {
            status=frame(bytes,size,index,channel,packet->clip[channel][1],
                packet->frame[channel][1],packet->conversion & (4u<<channel),b,&bh);
            if (status) return status;
            for (i=0;i<count;++i) a[i]=nba97_game_euler_blend(a[i],b[i],packet->weight[channel]);
            ah=half(lerp(ah,bh,packet->weight[channel]));
        }
        for (i=0;i<count;++i) pose.joint[base+i]=a[i];
        if (channel) pose.root_height=ah;
    }
    *out=pose; return NBA97_GAME_POSE_OK;
}
Nba97GamePoseResult nba97_game_foot_prefixes(const Nba97GameMocapIndex* index,
    uint16_t out[84], uint32_t* rows) {
    uint16_t local[84]; uint32_t total=0; unsigned slot;
    if (!index || !out || !rows || index->header_count>168) return NBA97_GAME_POSE_ARGUMENT;
    for (slot=0;slot<84;++slot) {
        unsigned ch, count=0;
        local[slot]=(uint16_t)total;
        for (ch=0;ch<2;++ch) {
            uint16_t ref=index->reference[ch][slot];
            if (ref==NBA97_GAME_MOCAP_NONE) continue;
            if (ref>=index->header_count) return NBA97_GAME_POSE_REFERENCE;
            if (index->header[ref].count>count) count=index->header[ref].count;
        }
        total+=count;
    }
    memcpy(out,local,sizeof local); *rows=total; return NBA97_GAME_POSE_OK;
}
static int32_t word(const uint8_t* p) {
    return s32((uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24);
}
static int32_t fixed_multiply(int32_t a, int32_t b) {
    /* AA788 signed64 multiply, add8000 including carry, then low32 of >>16. */
    uint64_t bits=(uint64_t)((int64_t)a*b)+0x8000u;
    return s32((uint32_t)(bits>>16));
}
Nba97GamePoseResult nba97_game_foot_offset(const uint8_t* zhots, size_t size,
    const uint16_t prefixes[84], const uint8_t* trig, size_t trig_size,
    const Nba97GameFootInput* input, Nba97GameFootOffset* out) {
    size_t at; int32_t x,z,sine,cosine,lo,hi; unsigned angle, quadrant;
    Nba97GameFootOffset result;
    if (!zhots || !prefixes || !trig || !input || !out) return NBA97_GAME_POSE_ARGUMENT;
    if (input->clip4a>=84) return NBA97_GAME_POSE_REFERENCE;
    at=((size_t)prefixes[input->clip4a]+input->frame54)*12u+(input->leg ? 9u:6u);
    if (at>size || 2u>size-at || trig_size<257u*4u) return NBA97_GAME_POSE_EXTENT;
    x=zhots[at]<128 ? zhots[at] : (int32_t)zhots[at]-256;
    z=zhots[at+1]<128 ? zhots[at+1] : (int32_t)zhots[at+1]-256;
    x=asr((uint32_t)(x*64)*input->scale_c6,8);
    z=asr((uint32_t)(z*64)*input->scale_c6,8);
    if (input->conversion9a&2u) x=s32(0u-(uint32_t)x);
    angle=(uint16_t)input->angle_a8; quadrant=(angle>>8)&3u; angle&=255u;
    lo=word(trig+4u*angle); hi=word(trig+4u*(256u-angle));
    sine=(quadrant&1u) ? hi:lo; cosine=(quadrant&1u) ? lo:hi;
    if (quadrant&2u) sine=s32(0u-(uint32_t)sine);
    if (quadrant==1u || quadrant==2u) cosine=s32(0u-(uint32_t)cosine);
    result.x=s32((uint32_t)fixed_multiply(cosine,x)+(uint32_t)fixed_multiply(sine,z));
    result.z=s32((uint32_t)fixed_multiply(cosine,z)-(uint32_t)fixed_multiply(sine,x));
    result.height=input->height10;
    *out=result; return NBA97_GAME_POSE_OK;
}
