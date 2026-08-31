#include "match_pose.hpp"
#include <stdexcept>
#include <utility>

namespace nba97 {
namespace {
constexpr unsigned half_offsets[]={0x46,0x48,0x4a,0x4c,0x50,0x52,0x54,0x56,
    0x60,0x62,0x64,0x66,0x84,0x86,0x88,0x8a,0x8c,0x8e,0x90,0x92,
    0x94,0x96,0x9a,0xec,0xc6,0xa8};
constexpr unsigned word_offsets[]={8,12,16,0x30,0x34};
static_assert(sizeof(half_offsets)/sizeof(*half_offsets)==NBA97_POSE_HALF_COUNT);
static_assert(sizeof(word_offsets)/sizeof(*word_offsets)==NBA97_POSE_WORD_COUNT);
std::int32_t signedWord(std::uint32_t value) {
    return value<=0x7fffffffu?std::int32_t(value):-1-std::int32_t(~value);
}
Nba97GamePoseEntity read(const MatchRuntimeRecord<244>& record) {
    Nba97GamePoseEntity out{};
    for(unsigned i=0;i<NBA97_POSE_HALF_COUNT;++i) {
        const auto v=record.read(half_offsets[i],2);
        out.half[i]=std::uint16_t(v.word);out.half_known|=std::uint32_t(v.known)<<i;
    }
    for(unsigned i=0;i<NBA97_POSE_WORD_COUNT;++i) {
        const auto v=record.read(word_offsets[i],4);
        out.word[i]=signedWord(v.word);out.word_known|=std::uint32_t(v.known)<<i;
    }
    const auto foot=record.read(0xe0,1);out.foot_e0=std::uint8_t(foot.word);out.foot_e0_known=foot.known;
    return out;
}
void applyChanges(MatchRuntimeRecord<244>& record,const Nba97GamePoseEntity& before,
                  const Nba97GamePoseEntity& after) {
    //57B18 keeps a live typed prefix rather than a separate write mask. Compare
    // fields and knownness, preserving unrelated bytes and partially-known spans.
    for(unsigned i=0;i<NBA97_POSE_HALF_COUNT;++i)
        if(before.half[i]!=after.half[i] || ((before.half_known^after.half_known)&(1u<<i)))
            record.write(half_offsets[i],2,{after.half[i],std::uint8_t((after.half_known>>i)&1)});
    for(unsigned i=0;i<NBA97_POSE_WORD_COUNT;++i)
        if(before.word[i]!=after.word[i] || ((before.word_known^after.word_known)&(1u<<i)))
            record.write(word_offsets[i],4,{std::uint32_t(after.word[i]),std::uint8_t((after.word_known>>i)&1)});
    if(before.foot_e0!=after.foot_e0 || before.foot_e0_known!=after.foot_e0_known)
        record.write(0xe0,1,{after.foot_e0,after.foot_e0_known});
}
}
MatchPoseResult prepareMatchRuntimePoses(MatchRuntimeState& live,const GameplayPoseResource& resources,
                                       Nba97GamePeriodReference render_first_fc654) {
    MatchPoseResult result;
    try {
        if(!live.accepted || !live.setup || !resources || resources->mocap()!=live.setup->mocap())
            throw std::runtime_error("pose resource generation differs from owned match motion");
        const auto first=live.entity_table[0];
        if(first.known!=1 || first.record>1) {
            result.result=first.known==0?NBA97_GAME_POSE_UNKNOWN:NBA97_GAME_POSE_REFERENCE;
            result.detail="pose request requires ten consecutive owned physical entities";return result;
        }
        if(render_first_fc654.known!=1 || render_first_fc654.record>1) {
            result.result=render_first_fc654.known==0?NBA97_GAME_POSE_UNKNOWN:NBA97_GAME_POSE_REFERENCE;
            result.detail="pose sampling requires a separate known FC654 render span";return result;
        }
        result.resources=resources;
        std::array<MatchRuntimeRecord<244>,11> candidate;
        for(unsigned i=0;i<11;++i)candidate[i]=live.entity[i].record;
        for(unsigned i=0;i<10;++i) {
            result.physical_entity[i]=first.record+i;
            result.render_entity[i]=render_first_fc654.record+i;
            result.requests[i]=read(candidate[first.record+i]);
        }
        const auto before=result.requests;
        result.result=nba97_game_pose_requests(&resources->mocap()->index(),result.requests.data(),
            GameplayPoseResources::resolveFoot,&result.resources,&result.completed_requests);
        if(result.result!=NBA97_GAME_POSE_OK) {
            result.detail="pose requests stopped at an unknown field, motion or foot-resource boundary";return result;
        }
        for(unsigned i=0;i<10;++i)applyChanges(candidate[first.record+i],before[i],result.requests[i]);
        for(unsigned i=0;i<10;++i) {
            //530FC consumes FC654's physical span, which can differ from the
            //57B18 request span. Preserve stale requests outside the latter.
            const auto render_request=read(candidate[result.render_entity[i]]);
            result.result=nba97_game_pose_packet(&render_request,&result.packet[i]);
            if(result.result!=NBA97_GAME_POSE_OK) {result.detail="pose packet needs a known consumed field";return result;}
            const auto& bytes=resources->mocap()->bytes();
            result.result=nba97_game_pose_sample(bytes.data(),bytes.size(),&resources->mocap()->index(),
                &result.packet[i],&result.pose[i]);
            if(result.result!=NBA97_GAME_POSE_OK) {result.detail="pose sample exceeds owned motion resource";return result;}
            ++result.sampled;
        }
        for(unsigned i=0;i<10;++i)live.entity[first.record+i].record=std::move(candidate[first.record+i]);
        result.published=true;
    } catch(const std::exception& e) {result.detail=e.what();}
    return result;
}
}
