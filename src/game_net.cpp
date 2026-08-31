#include "game_net.hpp"
#include "game_court_geometry.hpp"
namespace nba97 {
namespace {
int valid(Nba97GamePeriodValue v){return v.known>1||(!v.known&&v.word)?NBA97_BODY_ARGUMENT:v.known?NBA97_BODY_OK:NBA97_BODY_UNKNOWN;}
std::int16_t s16(std::uint32_t v){v&=65535;return std::int16_t(v<32768?std::int32_t(v):std::int32_t(v)-65536);}
}
int GameNetGeometry::callback(void* user,const Nba97PlayerMathRequest* q,Nba97GamePeriodValue* out){
    if(!user||!q||!out)return NBA97_BODY_ARGUMENT;
    return static_cast<GameNetGeometry*>(user)->apply(*q,*out);
}
int GameNetGeometry::apply(const Nba97PlayerMathRequest& q,Nba97GamePeriodValue& out){
    out={0,0};auto request=q;
    if(q.kind>=NBA97_NET_VECTOR_BASE){request.kind-=NBA97_NET_VECTOR_BASE;return player.root.vector.apply(request,out);}
    if(q.kind==NBA97_FRAME_PROJECT_ONE){request.kind=NBA97_ROOT_PROJECT;return player.root.apply(request,out);}
    if(q.kind!=NBA97_NET_AVERAGE_FOUR)return player.apply(request,out);
    int status=valid(average_scale4);
    for(auto v:{player.root.mac0,player.root.vector.flags,player.order_depth})if(valid(v)==NBA97_BODY_ARGUMENT)status=NBA97_BODY_ARGUMENT;
    for(auto v:player.root.depth){const int rc=valid(v);if(rc==NBA97_BODY_ARGUMENT||(status==NBA97_BODY_OK&&rc!=NBA97_BODY_OK))status=rc;if(v.known&&v.word>65535)status=NBA97_BODY_ARGUMENT;}
    if(status!=NBA97_BODY_OK)return status;
    GameCourtGeometry g;
    // This named AVSZ4 operation consumes only ZSF4 and depth FIFO. Other
    // temporary camera fields are not read, exported or claimed initialized.
    g.camera.known=true;g.camera.average_scale4=s16(average_scale4.word);
    for(unsigned i=0;i<4;++i)g.depth[i]={player.root.depth[i].word,player.root.depth[i].known};
    Nba97CourtValue result{};Nba97CourtMathRequest r{NBA97_COURT_AVERAGE_FOUR,q.pc,0,0};
    const int rc=g.apply(r,result);if(rc!=NBA97_COURT_COMPLETE)return rc==NBA97_COURT_UNKNOWN?NBA97_BODY_UNKNOWN:NBA97_BODY_ARGUMENT;
    player.root.mac0={g.mac[0].word,g.mac[0].known};player.root.vector.flags={g.flags.word,g.flags.known};player.order_depth={g.order_depth.word,g.order_depth.known};
    return NBA97_BODY_OK;
}
int GameNet::access(void* user,std::uint32_t pc,std::uint32_t address,unsigned width,unsigned kind,Nba97PlayerFrameValue* value){
    if(!user)return NBA97_BODY_ARGUMENT;
    auto& m=static_cast<GameNet*>(user)->memory;
    return m.access?m.access(m.user,pc,address,width,kind,value):NBA97_BODY_ARGUMENT;
}
int GameNet::math(void* user,const Nba97PlayerMathRequest* q,Nba97GamePeriodValue* out){
    if(!user||!q||!out)return NBA97_BODY_ARGUMENT;
    return static_cast<GameNet*>(user)->geometry.apply(*q,*out);
}
Nba97PlayerFrameContext GameNet::context(std::size_t budget){return {memory.access?access:nullptr,math,nullptr,this,budget};}
int GameNet::frame(std::size_t budget,Nba97GameNetProgress& p){auto c=context(budget);return nba97_game_net_frame(&c,&p);}
int GameNet::initialize(std::size_t budget,Nba97GameNetProgress& p){auto c=context(budget);return nba97_game_net_initialize(&c,&p);}
int GameNet::draw(std::size_t budget,Nba97GameNetProgress& p){auto c=context(budget);return nba97_game_net_draw(&c,&p);}
int GameNet::decode(std::uint32_t source,std::uint32_t destination,std::size_t budget,Nba97GamePeriodValue& length,Nba97GameNetProgress& p){auto c=context(budget);return nba97_game_net_decode(&c,source,destination,&length,&p);}
}
