#include "game_player_projection.hpp"
#include "game_court_geometry.hpp"
namespace nba97 {
namespace {
using Value=Nba97GamePeriodValue;
int valid(Value v){return v.known>1||(!v.known&&v.word)?NBA97_BODY_ARGUMENT:v.known?NBA97_BODY_OK:NBA97_BODY_UNKNOWN;}
std::int32_t s32(std::uint32_t v){return v<0x80000000u?std::int32_t(v):-1-std::int32_t(~v);}
std::int16_t s16(std::uint32_t v){v&=65535;return std::int16_t(v<32768?std::int32_t(v):std::int32_t(v)-65536);}
Nba97CourtValue court(Value v){return {v.word,v.known};}
Value player(Nba97CourtValue v){return {v.word,v.known};}
void check(Value v,int& status,bool consumed=true){const int rc=valid(v);if(rc==NBA97_BODY_ARGUMENT||(consumed&&status==NBA97_BODY_OK))status=rc;}
}
int GamePlayerProjectionGeometry::callback(void* user,const Nba97PlayerMathRequest* q,Value* out){
    if(!user||!q||!out)return NBA97_BODY_ARGUMENT;
    return static_cast<GamePlayerProjectionGeometry*>(user)->apply(*q,*out);
}
int GamePlayerProjectionGeometry::apply(const Nba97PlayerMathRequest& q,Value& out){
    out={0,0};
    if(q.kind==NBA97_PROJECTION_ROTATION||q.kind==NBA97_PROJECTION_TRANSLATION){
        auto r=q;r.kind=q.kind==NBA97_PROJECTION_ROTATION?NBA97_PLAYER_ROTATION:NBA97_PLAYER_TRANSLATION;
        return root.vector.apply(r,out);
    }
    if(q.kind==NBA97_PROJECTION_VERTEX){
        if(q.index>=6)return NBA97_BODY_ARGUMENT;
        Value& v=q.index<2?root.vector.vertex[q.index]:extra_vertex[q.index-2];
        if(valid(v)==NBA97_BODY_ARGUMENT)return NBA97_BODY_ARGUMENT;
        v={(q.index&1)?std::uint32_t(s16(q.word)):q.word,1};return NBA97_BODY_OK;
    }
    if(q.kind==NBA97_PROJECTION_SCREEN_LOAD){
        if(q.index>=3||valid(root.screen[q.index])==NBA97_BODY_ARGUMENT)return NBA97_BODY_ARGUMENT;
        root.screen[q.index]={q.word,1};return NBA97_BODY_OK;
    }
    if(q.kind==NBA97_PROJECTION_SCREEN){if(q.index>=3)return NBA97_BODY_ARGUMENT;out=root.screen[q.index];return valid(out);}
    if(q.kind==NBA97_PROJECTION_MAC0){out=root.mac0;return valid(out);}
    if(q.kind==NBA97_PROJECTION_DEPTH){out=order_depth;int rc=valid(out);return rc!=NBA97_BODY_OK?rc:out.word<=65535?NBA97_BODY_OK:NBA97_BODY_ARGUMENT;}
    int status=NBA97_BODY_OK;check(root.mac0,status,false);check(root.vector.flags,status,false);
    if(q.kind==NBA97_PROJECTION_AVERAGE_THREE){
        check(average_scale3,status);check(order_depth,status,false);
        for(unsigned i=1;i<4;++i){check(root.depth[i],status);if(root.depth[i].known&&root.depth[i].word>65535)status=NBA97_BODY_ARGUMENT;}
        if(status!=NBA97_BODY_OK)return status;
        const auto sum=root.depth[1].word+root.depth[2].word+root.depth[3].word;
        const std::int64_t scaled=std::int64_t(sum)*s16(average_scale3.word);
        std::uint32_t flags=0;if(scaled>2147483647)flags|=1u<<16;if(scaled<(-2147483647LL-1))flags|=1u<<15;
        // AVSZ3 clamps OTZ from the full product, independently of MAC0's
        // wrapped32-bit storage. Do not infer it from signed MAC0 afterwards.
        const std::int64_t z=scaled>=0?scaled/4096:-1-((-1-scaled)/4096);
        if(z<0||z>65535)flags|=1u<<18;
        order_depth={std::uint32_t(z<0?0:z>65535?65535:z),1};root.mac0={std::uint32_t(scaled),1};
        if(flags&0x7f87e000u)flags|=0x80000000u;
        root.vector.flags={flags,1};return NBA97_BODY_OK;
    }
    GameCourtGeometry g;
    if(q.kind==NBA97_PROJECTION_THREE){
        check(root.offset_x,status);check(root.offset_y,status);check(root.distance,status);check(root.depth_cue_a,status);check(root.depth_cue_b,status);
        for(auto v:root.vector.rotation)check(v,status);
        for(auto v:root.vector.translation)check(v,status);
        for(auto v:root.vector.vertex)check(v,status);
        for(auto v:extra_vertex)check(v,status);
        for(auto v:root.vector.mac)check(v,status,false);
        for(auto v:root.vector.ir)check(v,status,false);
        check(root.ir0,status,false);
        for(auto v:root.screen)check(v,status,false);
        for(auto v:root.depth)check(v,status,false);
        if(status!=NBA97_BODY_OK)return status;
        g.camera.known=true;
        for(unsigned i=0;i<9;++i)g.camera.rotation[i]=s16(root.vector.rotation[i/2].word>>((i&1)*16));
        for(unsigned i=0;i<3;++i)g.camera.translation[i]=s32(root.vector.translation[i].word);
        g.camera.offset_x=s32(root.offset_x.word);g.camera.offset_y=s32(root.offset_y.word);g.camera.distance=std::uint16_t(root.distance.word);
        g.camera.depth_cue_a=s16(root.depth_cue_a.word);g.camera.depth_cue_b=s32(root.depth_cue_b.word);
        for(unsigned i=0;i<6;++i)g.vertex[i]=court(i<2?root.vector.vertex[i]:extra_vertex[i-2]);
    }else if(q.kind==NBA97_PROJECTION_CLIP){
        for(auto v:root.screen)check(v,status);
        if(status!=NBA97_BODY_OK)return status;
    }else return NBA97_BODY_ARGUMENT;
    for(unsigned i=0;i<3;++i){g.screen[i]=court(root.screen[i]);g.mac[i+1]=court(root.vector.mac[i]);g.ir[i+1]=court(root.vector.ir[i]);}
    for(unsigned i=0;i<4;++i)g.depth[i]=court(root.depth[i]);
    g.mac[0]=court(root.mac0);g.ir[0]=court(root.ir0);g.flags=court(root.vector.flags);
    Nba97CourtMathRequest request{q.kind==NBA97_PROJECTION_THREE?NBA97_COURT_PROJECT_THREE:NBA97_COURT_NORMAL_CLIP,q.pc,0,0};Nba97CourtValue value{};
    const int rc=g.apply(request,value);if(rc!=NBA97_COURT_COMPLETE)return rc==NBA97_COURT_UNKNOWN?NBA97_BODY_UNKNOWN:NBA97_BODY_ARGUMENT;
    root.mac0=player(g.mac[0]);root.vector.flags=player(g.flags);
    if(q.kind==NBA97_PROJECTION_THREE){
        for(unsigned i=0;i<3;++i){root.screen[i]=player(g.screen[i]);root.vector.mac[i]=player(g.mac[i+1]);root.vector.ir[i]=player(g.ir[i+1]);}
        for(unsigned i=0;i<4;++i)root.depth[i]=player(g.depth[i]);
        root.ir0=player(g.ir[0]);
    }
    return NBA97_BODY_OK;
}
}
