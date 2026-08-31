#include "match_pose.hpp"
#include <cstdio>
#include <cstdlib>

using namespace nba97;
using Bytes=std::vector<std::uint8_t>;
static unsigned checks;
static void check(bool ok,const char* reason) {++checks;if(!ok){std::fprintf(stderr,"pose bridge %u: %s\n",checks,reason);std::exit(1);}}
static void word(Bytes& b,unsigned at,std::uint32_t v) {for(unsigned i=0;i<4;++i)b.at(at+i)=std::uint8_t(v>>(i*8));}
static GameplaySetupResource setup() {
    Bytes raw(1400);word(raw,0,8);word(raw,4,344);word(raw,8,700);word(raw,344,716);
    raw[700]=8;raw[703]=6;raw[707]=2;word(raw,708,100);
    raw[716]=0x40;raw[719]=6;raw[723]=4;word(raw,724,384);
    Bytes pack(2132);const char magic[]="NBA97PER";for(unsigned i=0;i<8;++i)pack[i]=std::uint8_t(magic[i]);
    word(pack,8,1);word(pack,12,2112);std::uint32_t crc=0xffffffffu;
    for(unsigned i=20;i<pack.size();++i) {
        crc^=pack[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    word(pack,16,~crc);return decodeGameplaySetup(pack,decode_gameplay_mocap(std::move(raw)));
}
static MatchRuntimeState state() {
    MatchRuntimeState s{};s.accepted=std::make_shared<const MatchSnapshot>();s.setup=setup();
    for(unsigned i=0;i<11;++i) {
        auto& record=s.entity[i].record;record.clearFromSource();record.put(0,4,i);
        record.put(8,4,i*100);record.put(12,4,std::uint32_t(-2000+int(i)*5));
        record.put(0x48,2,0xffff);record.put(0x4c,2,0xffff);record.put(0x50,2,1);record.put(0xc6,2,256);
        // Inactive secondary B frame and weight are not required by the source.
        record.write(0x92,2,{});record.write(0x96,2,{});s.entity_table[i]={0,1};
    }
    return s;
}
static std::uint64_t fingerprint(const MatchRuntimeState& s) {
    std::uint64_t value=1469598103934665603ull;
    for(const auto& e:s.entity) {
        for(auto byte:e.record.bytes){value^=byte;value*=1099511628211ull;}
        for(auto byte:e.record.known){value^=byte;value*=1099511628211ull;}
    }
    return value;
}
int main() {
    auto s=state();auto resource=decode_gameplay_pose_resources(s.setup->mocap(),Bytes(48),Bytes(1028));
    const auto original=s;auto result=prepareMatchRuntimePoses(s,resource,{0,1});
    check(result.published && result.result==NBA97_GAME_POSE_OK && result.completed_requests==10 && result.sampled==10,
        "all ten original requests, foot callbacks and render-value samples complete");
    for(unsigned i=0;i<10;++i) {
        check(result.physical_entity[i]==i && result.packet[i].clip[0][1]==0 && result.packet[i].weight[0]==128,
            "odd normalized primary frame synthesizes original midpoint");
        check(result.packet[i].clip[1][1]==65535 && !s.entity[i].record.read(0x92,2).known &&
            !s.entity[i].record.read(0x96,2).known,"inactive secondary B fields remain unknown and unused");
        check(s.entity[i].record.read(0xec,2).word==1 && s.entity[i].record.read(0x30,4).word==i*100,
            "actual foot callback populates stabilization cache");
        check(s.entity[i].record.read(0x14,2).word==original.entity[i].record.read(0x14,2).word &&
            s.entity[i].record.read(0x1a,1).word==original.entity[i].record.read(0x1a,1).word,
            "pose preparation does not invent velocity or actor transitions");
        for(const auto& angle:result.pose[i].joint)check(angle.x==0 && angle.y==0 && angle.z==0,"synthetic source pose has zero Euler values");
    }
    check(s.entity[10].record.bytes==original.entity[10].record.bytes &&
        s.entity[10].record.known==original.entity[10].record.known,"normal physical span leaves ball unchanged");
    for(unsigned call=0;call<2;++call)check(prepareMatchRuntimePoses(s,resource,{0,1}).published,"foot cache warmup");
    s.entity[0].record.put(8,4,999);result=prepareMatchRuntimePoses(s,resource,{0,1});
    check(result.published && s.entity[0].record.read(8,4).word==0 && s.entity[0].record.read(0xec,2).word==4,
        "fourth foot event restores cached position");
    s.entity[0].record.put(0xec,2,0x7fff);s.entity[0].record.put(8,4,123);
    result=prepareMatchRuntimePoses(s,resource,{0,1});
    check(result.published && s.entity[0].record.read(0xec,2).word==0x8000 && s.entity[0].record.read(0x30,4).word==123,
        "original signed counter wrap reenters early cache branch");
    auto failed=s;failed.entity[4].record.write(0x50,2,{});auto before=fingerprint(failed);
    result=prepareMatchRuntimePoses(failed,resource,{0,1});
    check(!result.published && result.result==NBA97_GAME_POSE_UNKNOWN && result.completed_requests==4 &&
        fingerprint(failed)==before,"unknown input keeps exact diagnostic prefix without publishing any frame");
    failed=s;failed.entity[0].record.put(0x50,2,65535);before=fingerprint(failed);
    result=prepareMatchRuntimePoses(failed,resource,{0,1});
    check(!result.published && result.result==NBA97_GAME_POSE_EXTENT && result.completed_requests==10 &&
        result.sampled==0 && fingerprint(failed)==before,"out-of-file sampling refuses without a clip clamp or prefix publication");
    before=fingerprint(s);auto other=decode_gameplay_pose_resources(setup()->mocap(),Bytes(48),Bytes(1028));
    result=prepareMatchRuntimePoses(s,other,{0,1});
    check(!result.published && fingerprint(s)==before,"pose generation mismatch refuses atomically");
    auto shifted=original;shifted.entity_table[0]={1,1};
    shifted.entity[0].record.put(0x86,2,65535);shifted.entity[0].record.put(0x8a,2,65535);
    const auto entity0=shifted.entity[0].record;
    result=prepareMatchRuntimePoses(shifted,resource,{0,1});
    check(result.published && result.physical_entity[0]==1 && result.physical_entity[9]==10 &&
        result.render_entity[0]==0 && result.render_entity[9]==9 && shifted.entity[0].record.bytes==entity0.bytes,
        "request and render pointers independently control their physical spans");
    check(result.packet[0].clip[0][1]==65535 && result.packet[1].clip[0][1]==0,
        "render keeps stale entity0 request while entity1 receives fresh midpoint request");
    before=fingerprint(shifted);result=prepareMatchRuntimePoses(shifted,resource,{});
    check(!result.published && result.result==NBA97_GAME_POSE_UNKNOWN && fingerprint(shifted)==before,
        "unknown render pointer does not reuse the request pointer");
    result.resources.reset();resource.reset();
    check(other->footRows()==4,"independent immutable resource generation survives replacement");
    std::printf("MATCH POSE PASS: %u checks; physical requests, sampled values, source foot locks and atomic frame refusal\n",checks);
    return 0;
}
