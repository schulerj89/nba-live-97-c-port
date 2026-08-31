#include "game_player_root.hpp"
#include "game_court_geometry.hpp"
namespace nba97 {
namespace {
int valid(Nba97GamePeriodValue v){return v.known>1||(!v.known&&v.word)?NBA97_BODY_ARGUMENT:v.known?NBA97_BODY_OK:NBA97_BODY_UNKNOWN;}
std::int32_t s32(std::uint32_t v){return v<0x80000000u?std::int32_t(v):-1-std::int32_t(~v);}
std::int16_t s16(std::uint32_t v){v&=65535;return std::int16_t(v<32768?std::int32_t(v):std::int32_t(v)-65536);}
Nba97CourtValue court(Nba97GamePeriodValue v){return {v.word,v.known};}
Nba97GamePeriodValue player(Nba97CourtValue v){return {v.word,v.known};}
}
int GamePlayerRootGeometry::callback(void* user,const Nba97PlayerMathRequest* request,Nba97GamePeriodValue* result){
    if(!user||!request||!result)return NBA97_BODY_ARGUMENT;
    return static_cast<GamePlayerRootGeometry*>(user)->apply(*request,*result);
}
int GamePlayerRootGeometry::apply(const Nba97PlayerMathRequest& r,Nba97GamePeriodValue& out){
    out={0,0};
    if(r.kind<=NBA97_PLAYER_MAC)return vector.apply(r,out);
    if(r.kind==NBA97_ROOT_SCREEN)out=screen[2];
    else if(r.kind==NBA97_ROOT_IR0)out=ir0;
    else if(r.kind==NBA97_ROOT_FLAGS)out=vector.flags;
    else if(r.kind==NBA97_ROOT_DEPTH)out=depth[3];
    else if(r.kind!=NBA97_ROOT_PROJECT)return NBA97_BODY_ARGUMENT;
    if(r.kind!=NBA97_ROOT_PROJECT)return valid(out);
    int status=NBA97_BODY_OK;
    const auto check=[&status](Nba97GamePeriodValue v){int result=valid(v);if(result==NBA97_BODY_ARGUMENT||status==NBA97_BODY_OK)status=result;};
    check(offset_x);check(offset_y);check(distance);check(depth_cue_a);check(depth_cue_b);
    for(auto v:vector.rotation)check(v);
    for(auto v:vector.translation)check(v);
    for(auto v:vector.vertex)check(v);
    if(status!=NBA97_BODY_OK)return status;
    // Original RTPS uses no AVSZ4/controlZSF4/IR0 input. The local adapter's
    // unused fields are not exported or asserted to be initialized device state.
    GameCourtGeometry g;g.camera.known=true;
    for(unsigned i=0;i<9;++i)g.camera.rotation[i]=s16(vector.rotation[i/2].word>>((i&1)*16));
    for(unsigned i=0;i<3;++i)g.camera.translation[i]=s32(vector.translation[i].word);
    g.camera.offset_x=s32(offset_x.word);g.camera.offset_y=s32(offset_y.word);g.camera.distance=std::uint16_t(distance.word);
    g.camera.depth_cue_a=s16(depth_cue_a.word);g.camera.depth_cue_b=s32(depth_cue_b.word);
    for(unsigned i=0;i<2;++i)g.vertex[i]=court(vector.vertex[i]);
    for(unsigned i=0;i<3;++i){g.screen[i]=court(screen[i]);g.mac[i+1]=court(vector.mac[i]);g.ir[i+1]=court(vector.ir[i]);}
    for(unsigned i=0;i<4;++i)g.depth[i]=court(depth[i]);
    g.mac[0]=court(mac0);g.ir[0]=court(ir0);g.flags=court(vector.flags);
    // Refuse invalid metadata on overwritten retained registers, too. Unknown
    // values may be replaced, but noncanonical metadata is never laundered.
    for(auto v:vector.mac)if(valid(v)==NBA97_BODY_ARGUMENT)return NBA97_BODY_ARGUMENT;
    for(auto v:vector.ir)if(valid(v)==NBA97_BODY_ARGUMENT)return NBA97_BODY_ARGUMENT;
    if(valid(mac0)==NBA97_BODY_ARGUMENT||valid(ir0)==NBA97_BODY_ARGUMENT||valid(vector.flags)==NBA97_BODY_ARGUMENT)return NBA97_BODY_ARGUMENT;
    Nba97CourtMathRequest q{NBA97_COURT_PROJECT_ONE,r.pc,0,0};Nba97CourtValue result{};
    const int rc=g.apply(q,result);if(rc!=NBA97_COURT_COMPLETE)return rc==NBA97_COURT_UNKNOWN?NBA97_BODY_UNKNOWN:NBA97_BODY_ARGUMENT;
    for(unsigned i=0;i<3;++i){screen[i]=player(g.screen[i]);vector.mac[i]=player(g.mac[i+1]);vector.ir[i]=player(g.ir[i+1]);}
    for(unsigned i=0;i<4;++i)depth[i]=player(g.depth[i]);
    mac0=player(g.mac[0]);ir0=player(g.ir[0]);vector.flags=player(g.flags);return NBA97_BODY_OK;
}
}
