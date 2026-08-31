#include "game_player_geometry.hpp"
#include <cstdint>
namespace nba97 {
namespace {
using W=std::uint32_t;using L=std::int64_t;
int valid(Nba97GamePeriodValue v){return v.known>1||(!v.known&&v.word)?NBA97_BODY_ARGUMENT:v.known?NBA97_BODY_OK:NBA97_BODY_UNKNOWN;}
std::int32_t signedWord(W v){return v<=0x7fffffffu?std::int32_t(v):-1-std::int32_t(~v);}
std::int32_t signedHalf(W v){v&=65535;return v<32768?std::int32_t(v):std::int32_t(v)-65536;}
void flagOverflow(L v,unsigned row,W& f){if(v<-(L(1)<<43))f|=1u<<(27-row);if(v>((L(1)<<43)-1))f|=1u<<(30-row);}
L narrow44(L v,unsigned row,W& f){flagOverflow(v,row,f);auto u=std::uint64_t(v)&0xfffffffffffULL;return u<0x80000000000ULL?L(u):L(u)-0x100000000000LL;}
L shift12(L v){return v>=0?v/4096:-1-((-1-v)/4096);}
}
int GamePlayerGeometry::callback(void* p,const Nba97PlayerMathRequest* r,Nba97GamePeriodValue* out){
    if(!p||!r||!out)return NBA97_BODY_ARGUMENT;
    return static_cast<GamePlayerGeometry*>(p)->apply(*r,*out);
}
int GamePlayerGeometry::apply(const Nba97PlayerMathRequest& r,Nba97GamePeriodValue& out){
    out={0,0};Nba97GamePeriodValue* destination=nullptr;
    if(r.kind==NBA97_PLAYER_ROTATION){if(r.index>=5)return NBA97_BODY_ARGUMENT;destination=&rotation[r.index];}
    else if(r.kind==NBA97_PLAYER_TRANSLATION){if(r.index>=3)return NBA97_BODY_ARGUMENT;destination=&translation[r.index];}
    else if(r.kind==NBA97_PLAYER_VERTEX){if(r.index>=2)return NBA97_BODY_ARGUMENT;destination=&vertex[r.index];}
    if(destination){
        if(valid(*destination)==NBA97_BODY_ARGUMENT)return NBA97_BODY_ARGUMENT;
        const bool half=(r.kind==NBA97_PLAYER_ROTATION&&r.index==4)||(r.kind==NBA97_PLAYER_VERTEX&&r.index==1);
        *destination={half?W(signedHalf(r.word)):r.word,1};return NBA97_BODY_OK;
    }
    if(r.kind==NBA97_PLAYER_IR||r.kind==NBA97_PLAYER_MAC){
        if(r.index>=3)return NBA97_BODY_ARGUMENT;
        out=r.kind==NBA97_PLAYER_IR?ir[r.index]:mac[r.index];return valid(out);
    }
    if(r.kind!=NBA97_PLAYER_ROTATE&&r.kind!=NBA97_PLAYER_TRANSFORM)return NBA97_BODY_ARGUMENT;
    int result=NBA97_BODY_OK;
    const auto check=[&result](Nba97GamePeriodValue v){const int t=valid(v);if(t==NBA97_BODY_ARGUMENT||result==NBA97_BODY_OK)result=t;};
    for(auto v:rotation)check(v);
    for(auto v:vertex)check(v);
    if(r.kind==NBA97_PLAYER_TRANSFORM)for(auto v:translation)check(v);
    if(result!=NBA97_BODY_OK)return result;
    for(auto v:ir)if(valid(v)==NBA97_BODY_ARGUMENT)return NBA97_BODY_ARGUMENT;
    for(auto v:mac)if(valid(v)==NBA97_BODY_ARGUMENT)return NBA97_BODY_ARGUMENT;
    if(valid(flags)==NBA97_BODY_ARGUMENT)return NBA97_BODY_ARGUMENT;
    const std::int32_t v[3]={signedHalf(vertex[0].word),signedHalf(vertex[0].word>>16),signedHalf(vertex[1].word)};
    W f=0;
    for(unsigned row=0;row<3;++row){
        L sum=r.kind==NBA97_PLAYER_TRANSFORM?L(signedWord(translation[row].word))*4096:0;
        for(unsigned col=0;col<3;++col){const unsigned i=row*3+col;sum+=L(signedHalf(rotation[i/2].word>>((i&1)*16)))*v[col];
            // MVMVA preserves44-bit intermediate wrap after the first two
            // additions, but checks the final sum before shift/low32 storage.
            if(col<2)sum=narrow44(sum,row,f);else flagOverflow(sum,row,f);
        }
        const auto m=signedWord(W(shift12(sum)));mac[row]={W(m),1};
        std::int32_t limited=m;if(m<-32768){limited=-32768;f|=1u<<(24-row);}else if(m>32767){limited=32767;f|=1u<<(24-row);}
        ir[row]={W(limited),1};
    }
    if(f&0x7f87e000u)f|=0x80000000u;
    flags={f,1};return NBA97_BODY_OK;
}
}
