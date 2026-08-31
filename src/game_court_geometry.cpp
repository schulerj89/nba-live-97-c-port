#include "game_court_geometry.hpp"
#include "zdomf_projection.hpp"
#include <algorithm>
#include <cstdint>

namespace nba97 {
namespace {
using Word=std::uint32_t;
using Wide=std::int64_t;
std::int32_t signedWord(Word x){return x<0x80000000u?std::int32_t(x):-1-std::int32_t(~x);}
std::int32_t signedHalf(Word x){x&=65535;return x<32768?std::int32_t(x):std::int32_t(x)-65536;}
Wide floorShift(Wide x,unsigned n){const Wide d=Wide(1)<<n;return x>=0?x/d:-1-((-1-x)/d);}
Nba97CourtValue value(Word x){return {x,1};}
int valid(Nba97CourtValue v){
    if(v.known>1||(!v.known&&v.word))return NBA97_COURT_ARGUMENT;
    return v.known?NBA97_COURT_COMPLETE:NBA97_COURT_UNKNOWN;
}
void overflow(Wide x,unsigned row,Word& flags){
    const Wide lower=row?-(Wide(1)<<43):-(Wide(1)<<31);
    const Wide upper=-lower-1;
    if(x<lower)flags|=1u<<(row?28-row:15);
    if(x>upper)flags|=1u<<(row?31-row:16);
}
Wide keep44(Wide x,unsigned row,Word& flags){
    overflow(x,row,flags);
    const auto low=std::uint64_t(x)&((std::uint64_t(1)<<44)-1);
    return (low&(std::uint64_t(1)<<43))?Wide(low)- (Wide(1)<<44):Wide(low);
}
std::int32_t limit(Wide x,std::int32_t lower,std::int32_t upper,unsigned bit,Word& flags){
    if(x<lower){flags|=1u<<bit;return lower;}
    if(x>upper){flags|=1u<<bit;return upper;}
    return std::int32_t(x);
}
void finishFlags(GameCourtGeometry& g,Word flags){
    if(flags&0x7f87e000u)flags|=0x80000000u;
    g.flags=value(flags);
}
void project(GameCourtGeometry& g,unsigned index,bool last,Word& flags){
    const Word xy=g.vertex[index*2].word;
    const std::array<std::int32_t,3> v={signedHalf(xy),signedHalf(xy>>16),signedHalf(g.vertex[index*2+1].word)};
    std::array<std::int32_t,3> result{};
    for(unsigned row=0;row<3;++row){
        Wide sum=Wide(g.camera.translation[row])*4096;
        sum=keep44(sum+Wide(g.camera.rotation[row*3])*v[0],row+1,flags);
        sum=keep44(sum+Wide(g.camera.rotation[row*3+1])*v[1],row+1,flags);
        sum+=Wide(g.camera.rotation[row*3+2])*v[2];
        overflow(sum,row+1,flags);
        result[row]=signedWord(Word(floorShift(sum,12)));
        g.mac[row+1]=value(Word(result[row]));
        g.ir[row+1]=value(Word(limit(result[row],-32768,32767,24-row,flags)));
    }
    const Word z=Word(limit(result[2],0,65535,18,flags));
    for(unsigned i=0;i<3;++i)g.depth[i]=g.depth[i+1];
    g.depth[3]=value(z);
    if(z*2u<=g.camera.distance)flags|=1u<<17;
    const Word quotient=zdomf_gte_unr_divide(g.camera.distance,z);
    const Wide sx=Wide(quotient)*signedWord(g.ir[1].word)+g.camera.offset_x;
    const Wide sy=Wide(quotient)*signedWord(g.ir[2].word)+g.camera.offset_y;
    overflow(sx,0,flags);overflow(sy,0,flags);
    const Word x=Word(limit(floorShift(sx,16),-1024,1023,14,flags))&65535;
    const Word y=Word(limit(floorShift(sy,16),-1024,1023,13,flags))&65535;
    g.screen[0]=g.screen[1];g.screen[1]=g.screen[2];g.screen[2]=value(x|(y<<16));
    if(last){
        const Wide cue=Wide(quotient)*g.camera.depth_cue_a+g.camera.depth_cue_b;
        overflow(cue,0,flags);g.mac[0]=value(Word(cue));
        g.ir[0]=value(Word(limit(signedWord(Word(floorShift(cue,12))),0,4096,12,flags)));
    }
}
}

int GameCourtGeometry::callback(void* user,const Nba97CourtMathRequest* request,Nba97CourtValue* result){
    if(!user||!request||!result)return NBA97_COURT_ARGUMENT;
    return static_cast<GameCourtGeometry*>(user)->apply(*request,*result);
}
int GameCourtGeometry::apply(const Nba97CourtMathRequest& request,Nba97CourtValue& out){
    Word status_flags=0;out={0,0};
    switch(request.kind){
    case NBA97_COURT_LOAD_ROTATION_WORD:
        if(request.index>=5)return NBA97_COURT_ARGUMENT;
        camera.rotation[request.index*2]=std::int16_t(signedHalf(request.word));
        if(request.index<4)camera.rotation[request.index*2+1]=std::int16_t(signedHalf(request.word>>16));
        return NBA97_COURT_COMPLETE;
    case NBA97_COURT_LOAD_TRANSLATION_WORD:
        if(request.index>=3)return NBA97_COURT_ARGUMENT;
        camera.translation[request.index]=signedWord(request.word);
        // Loading the matrix does not establish unknown OFX/OFY/H/DQ/ZSF4.
        // The owning renderer must provide those controls before projection.
        return NBA97_COURT_COMPLETE;
    case NBA97_COURT_LOAD_VERTEX_WORD:
        if(request.index>=vertex.size())return NBA97_COURT_ARGUMENT;
        vertex[request.index]=value((request.index&1)?Word(signedHalf(request.word)):request.word);
        return NBA97_COURT_COMPLETE;
    case NBA97_COURT_SCREEN:
        if(request.index>=screen.size())return NBA97_COURT_ARGUMENT;
        out=screen[request.index];return valid(out);
    case NBA97_COURT_LEADING_BITS:
        out=leading_bits;
        if(valid(out)!=NBA97_COURT_COMPLETE)return valid(out);
        // LZCR is a count, never a signed projection FLAG value. Keep the
        // original game's wrong-register check; do not replace it with FLAG.
        return out.word<=32?NBA97_COURT_COMPLETE:NBA97_COURT_ARGUMENT;
    case NBA97_COURT_ORDER_DEPTH:
        out=order_depth;
        if(valid(out)!=NBA97_COURT_COMPLETE)return valid(out);
        return out.word<=65535?NBA97_COURT_COMPLETE:NBA97_COURT_ARGUMENT;
    case NBA97_COURT_PROJECT_ONE:
    case NBA97_COURT_PROJECT_THREE: {
        if(!camera.known)return NBA97_COURT_UNKNOWN;
        const unsigned n=request.kind==NBA97_COURT_PROJECT_ONE?1:3;
        for(unsigned i=0;i<n*2;++i){const int result=valid(vertex[i]);if(result!=NBA97_COURT_COMPLETE)return result;}
        // Validate metadata being shifted, but do not require old FIFO values
        // to be known: the real projection replaces only the newest entries.
        for(auto v:screen)if(valid(v)==NBA97_COURT_ARGUMENT)return NBA97_COURT_ARGUMENT;
        for(auto v:depth)if(valid(v)==NBA97_COURT_ARGUMENT)return NBA97_COURT_ARGUMENT;
        for(unsigned i=0;i<n;++i)project(*this,i,i+1==n,status_flags);
        finishFlags(*this,status_flags);return NBA97_COURT_COMPLETE;
    }
    case NBA97_COURT_NORMAL_CLIP: {
        std::array<std::int32_t,3> x{},y{};
        for(unsigned i=0;i<3;++i){const int result=valid(screen[i]);if(result!=NBA97_COURT_COMPLETE)return result;
            x[i]=signedHalf(screen[i].word);y[i]=signedHalf(screen[i].word>>16);}
        const Wide determinant=Wide(x[0])*(y[1]-y[2])+Wide(x[1])*(y[2]-y[0])+Wide(x[2])*(y[0]-y[1]);
        overflow(determinant,0,status_flags);mac[0]=value(Word(determinant));
        finishFlags(*this,status_flags);out=mac[0];return NBA97_COURT_COMPLETE;
    }
    case NBA97_COURT_AVERAGE_FOUR: {
        if(!camera.known)return NBA97_COURT_UNKNOWN;
        Word sum=0;
        for(auto v:depth){const int result=valid(v);if(result!=NBA97_COURT_COMPLETE)return result;
            if(v.word>65535)return NBA97_COURT_ARGUMENT;
            sum+=v.word;}
        const Wide scaled=Wide(sum)*camera.average_scale4;
        overflow(scaled,0,status_flags);mac[0]=value(Word(scaled));
        order_depth=value(Word(limit(floorShift(scaled,12),0,65535,18,status_flags)));
        finishFlags(*this,status_flags);return NBA97_COURT_COMPLETE;
    }
    default:return NBA97_COURT_ARGUMENT;
    }
}
}
