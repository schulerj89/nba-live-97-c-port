#include "gameplay_pose.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static unsigned checks=0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"line%d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using Bytes=std::vector<std::uint8_t>;
static void put(Bytes& b,std::size_t at,std::uint32_t v,unsigned size=4) {
    for (unsigned i=0;i<size;++i) b.at(at+i)=static_cast<std::uint8_t>(v>>(8*i));
}
static Bytes fixture() {
    Bytes b(1300); put(b,0,8); put(b,4,344);
    for (unsigned i=0;i<84;++i) { put(b,8+i*4,1200); put(b,344+i*4,1212); }
    put(b,1200,8,2); b[1203]=6; b[1207]=2;
    put(b,1208,static_cast<std::uint32_t>(704-1200));
    b[1219]=1; put(b,1220,static_cast<std::uint32_t>(896-1212));
    for (unsigned f=0;f<2;++f) for (unsigned j=0;j<12;++j) {
        auto at=704+f*96+j*8;
        put(b,at,10+j+f*100,2); put(b,at+2,20+j,2); put(b,at+4,30+j,2);
        put(b,at+6,0xbeef,2); // Source never validates padding/marker values.
    }
    put(b,896,0x1234,2); put(b,898,static_cast<std::uint32_t>(-70),2);
    for (unsigned j=0;j<8;++j) {
        put(b,900+j*8,1000+j,2); put(b,902+j*8,1100+j,2); put(b,904+j*8,1200+j,2);
        put(b,906+j*8,0xcafe,2);
    }
    return b;
}
static void sample_tests() {
    auto bytes=fixture();auto motion=nba97::decode_gameplay_mocap(bytes);
    const auto& index=motion->index();
    CHECK(index.header_count==2 && index.header[0].data_offset==704);
    CHECK(index.header[0].count==4 && index.header[1].count==1);
    Nba97GamePosePacket p{};p.clip[0][1]=0xffff;p.clip[1][1]=0x8000;
    Nba97GamePose pose{};
    CHECK(nba97_game_pose_sample(bytes.data(),bytes.size(),&index,&p,&pose)==NBA97_GAME_POSE_OK);
    CHECK(pose.joint[8].x==10 && pose.joint[19].z==41 && pose.root_height==-70);
    CHECK(pose.joint[0].x==1000 && pose.joint[7].y==1107);
    p.clip[0][0]=83;p.clip[1][0]=83;
    Nba97GamePose aliased{};
    CHECK(nba97_game_pose_sample(bytes.data(),bytes.size(),&index,&p,&aliased)==0);
    CHECK(std::memcmp(&pose,&aliased,sizeof pose)==0);
    p.conversion=3;
    CHECK(nba97_game_pose_sample(bytes.data(),bytes.size(),&index,&p,&pose)==0);
    CHECK(pose.joint[8+8].x==2048-14 && pose.joint[8+4].z==2048-38);
    CHECK(pose.joint[4].x==2048-1000 && pose.joint[0].z==2048-1204);
    CHECK(pose.root_height==-70); // root+0 and markers are not render values.
    p.conversion=0;p.clip[0][1]=0;p.frame[0][1]=1;p.weight[0]=65535;
    CHECK(nba97_game_pose_sample(bytes.data(),bytes.size(),&index,&p,&pose)==0);
    CHECK(pose.joint[8].x==25609); // 10 + floor(100*65535/256), no weight clamp.
    p.clip[0][1]=0xffff;p.frame[0][0]=2;
    CHECK(nba97_game_pose_sample(bytes.data(),bytes.size(),&index,&p,&pose)==0);
    CHECK(pose.joint[8].x==0x1234); // Out-of-count but in-file source read retained.
    p.frame[0][0]=5;
    CHECK(nba97_game_pose_sample(bytes.data(),1280,&index,&p,&pose)==0); // exact end
    const auto before=pose;
    CHECK(nba97_game_pose_sample(bytes.data(),1279,&index,&p,&pose)==NBA97_GAME_POSE_EXTENT);
    CHECK(std::memcmp(&pose,&before,sizeof pose)==0);
    p.frame[0][0]=65535;
    CHECK(nba97_game_pose_sample(bytes.data(),bytes.size(),&index,&p,&pose)==NBA97_GAME_POSE_EXTENT);
    p.clip[0][0]=84;
    CHECK(nba97_game_pose_sample(bytes.data(),bytes.size(),&index,&p,&pose)==NBA97_GAME_POSE_REFERENCE);
    CHECK(std::memcmp(&pose,&before,sizeof pose)==0);
    CHECK(nba97_game_pose_sample(nullptr,0,&index,&p,&pose)==NBA97_GAME_POSE_ARGUMENT);
    auto corrupt=index;corrupt.reference[0][0]=NBA97_GAME_MOCAP_NONE;p.clip[0][0]=0;
    CHECK(nba97_game_pose_sample(bytes.data(),bytes.size(),&corrupt,&p,&pose)==NBA97_GAME_POSE_REFERENCE);
}
static void blend_tests() {
    auto a=Nba97GameEuler{10,20,30},b=Nba97GameEuler{110,20,30};
    CHECK(nba97_game_euler_blend(a,b,0).x==10);
    CHECK(nba97_game_euler_blend(a,b,128).x==60);
    CHECK(nba97_game_euler_blend(a,b,256).x==110);
    CHECK(nba97_game_euler_blend(a,b,65535).x==25609);
    CHECK(nba97_game_euler_blend({-1,0,0},{0,0,0},128).x==-1); // arithmetic floor
    CHECK(nba97_game_euler_blend({0,0,0},{4096,0,0},0).x==4096); // A may be adjusted even at0.
}
static void foot_and_owner_tests() {
    auto mocap=nba97::decode_gameplay_mocap(fixture());
    std::uint16_t prefix[84]{};std::uint32_t rows=0;
    CHECK(nba97_game_foot_prefixes(&mocap->index(),prefix,&rows)==0);
    CHECK(rows==336 && prefix[1]==4 && prefix[83]==332);
    Bytes foot(rows*12),trig(1028);foot[6]=0xfe;foot[7]=3;foot[9]=7;foot[10]=0xff;
    put(trig,256*4,65536); // Explicit synthetic quarter-wave endpoint, not fabricated retail data.
    Nba97GameFootInput input{};input.scale_c6=256;input.height10=-123;
    Nba97GameFootOffset offset{};
    CHECK(nba97_game_foot_offset(foot.data(),foot.size(),prefix,trig.data(),trig.size(),&input,&offset)==0);
    CHECK(offset.x==-128 && offset.z==192 && offset.height==-123);
    input.conversion9a=2;
    CHECK(nba97_game_foot_offset(foot.data(),foot.size(),prefix,trig.data(),trig.size(),&input,&offset)==0);
    CHECK(offset.x==128 && offset.z==192);
    input.angle_a8=256;
    CHECK(nba97_game_foot_offset(foot.data(),foot.size(),prefix,trig.data(),trig.size(),&input,&offset)==0);
    CHECK(offset.x==192 && offset.z==-128);
    input.angle_a8=0;input.conversion9a=0;input.leg=0xffffffff;
    CHECK(nba97_game_foot_offset(foot.data(),11,prefix,trig.data(),trig.size(),&input,&offset)==0);
    CHECK(offset.x==448 && offset.z==-64);
    const auto before=offset;
    CHECK(nba97_game_foot_offset(foot.data(),10,prefix,trig.data(),trig.size(),&input,&offset)==NBA97_GAME_POSE_EXTENT);
    CHECK(std::memcmp(&before,&offset,sizeof offset)==0);
    input.frame54=65535;
    CHECK(nba97_game_foot_offset(foot.data(),foot.size(),prefix,trig.data(),trig.size(),&input,&offset)==NBA97_GAME_POSE_EXTENT);
    auto current=nba97::decode_gameplay_pose_resources(mocap,foot,trig);auto retained=current;
    CHECK(current->footRows()==336 && current->mocap()==mocap);
    Nba97GamePosePacket packet{};packet.clip[0][1]=0xffff;packet.clip[1][1]=0xffff;
    auto sampled=current->sample(packet);CHECK(sampled.joint[8].x==10 && sampled.root_height==-70);
    input.frame54=0;CHECK(current->footOffset(input).x==448);
    foot[6]=42;CHECK(current->footBytes()[6]==0xfe); // owner copied input bytes
    bool refused=false;
    packet.frame[0][0]=65535;
    try { (void)current->sample(packet); } catch(const std::out_of_range&) { refused=true; }
    CHECK(refused);refused=false;
    try { current=nba97::decode_gameplay_pose_resources(mocap,Bytes(1),trig); } catch(const std::runtime_error&) { refused=true; }
    CHECK(refused && current==retained);
    refused=false;
    try { current=nba97::decode_gameplay_pose_resources(mocap,foot,Bytes(1027)); } catch(const std::runtime_error&) { refused=true; }
    CHECK(refused && current==retained);
    Nba97GamePoseEntity e{};e.half_known=(1u<<26)-1;e.word_known=31;e.half[NBA97_POSE_C6]=256;
    CHECK(nba97::GameplayPoseResources::resolveFoot(&current,0,&e,0,&offset));
    CHECK(offset.x==-128 && offset.z==192);
    e.half_known&=~(1u<<NBA97_POSE_C6);const auto previous=offset;
    CHECK(!nba97::GameplayPoseResources::resolveFoot(&current,0,&e,0,&offset));
    CHECK(std::memcmp(&previous,&offset,sizeof offset)==0);
    mocap.reset();current.reset();CHECK(retained->mocap()->bytes().size()==1300);
}
int main() { sample_tests();blend_tests();foot_and_owner_tests();std::printf("game_pose_sample: %u checks passed\n",checks); }
