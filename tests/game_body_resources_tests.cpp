#include "game_body_resources.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nba97;
using Ref=Nba97GameBodyReference;
unsigned checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(std::to_string(checks)+": "+why);}
template<class F> void refuses(F action,const char* why){bool threw=false;try{action();}catch(const std::exception&){threw=true;}check(threw,why);}
Ref offset(Ref ref,std::uint32_t delta){ref.offset+=delta;return ref;}
bool same(Ref a,Ref b){return a.known==b.known&&a.allocation==b.allocation&&a.offset==b.offset;}
void word(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));}
struct Body {
    GameBodyBytes input{std::vector<std::uint8_t>(30000,0x5a),{},0};
    std::array<std::array<std::uint32_t,20>,5> headers{};
    Body(){
        auto& bytes=input.bytes;word(bytes,0,3);word(bytes,4,3);std::uint32_t cursor=12;
        for(unsigned player=0;player<5;++player){
            for(unsigned part=0;part<20;++part){cursor+=12;headers[player][part]=cursor;word(bytes,cursor,1);cursor+=16;
                if(!player)cursor+=24;cursor+=64;}
            cursor+=4;word(bytes,cursor,1);word(bytes,cursor+4,1);cursor+=60;
            cursor+=24+4;
            const auto primaryIndex=cursor;cursor+=12;const auto secondaryIndex=cursor;cursor+=12+4;
            const auto primaryA=cursor;cursor+=12;const auto primaryB=cursor;cursor+=12;
            const auto secondaryA=cursor;cursor+=12;const auto secondaryB=cursor;cursor+=12+4;
            for(unsigned i=0;i<3;++i){
                word(bytes,primaryIndex+i*4,i);word(bytes,secondaryIndex+i*4,i);
                word(bytes,primaryA+i*4,0x01000008+i*8);word(bytes,primaryB+i*4,0x13000008+i*8);
                word(bytes,secondaryA+i*4,0x01000008+i*8);word(bytes,secondaryB+i*4,0xff000008+i*8);
            }
            cursor+=64+4+64+4;
            const auto groups=cursor;cursor+=100;
            for(unsigned group=0;group<6;++group){word(bytes,groups+group*16,1);cursor+=68;}
        }
        bytes.resize(cursor);
    }
};
GameBodyBytes contexts(){return {std::vector<std::uint8_t>(10*GameBodyResources::ContextStride,0x71),{},0};}
void ownership(){
    Body home,away;auto result=prepareGameBodyResources(contexts(),home.input,away.input);
    check(result.result==NBA97_BODY_OK&&result.resource&&result.sidesCompleted==2,"both original five-player calls complete");
    check(result.side[0].cursor.offset==home.input.bytes.size()&&result.side[1].cursor.offset==away.input.bytes.size(),"two independent logical payload cursors");
    check(result.journal.size()==result.side[0].writes+result.side[1].writes,"both source journals retained");
    auto& resource=*result.resource;
    for(unsigned player=0;player<10;++player){
        const unsigned id=player<5?GameBodyResources::Home:GameBodyResources::Away;
        const auto& source=player<5?home:away;
        for(unsigned part=0;part<20;++part){
            const auto header=resource.partHeader(player,part);
            check(same(header,{id,source.headers[player%5][part],1}),"header uses exact side allocation and cursor");
            const auto xyz=resource.referenceAt(offset(header,4));
            check(same(xyz,resource.referenceAt(offset(resource.partHeader(player<5?0:5,part),4))),"XYZ shared within side");
            const auto a=resource.referenceAt(offset(header,8)),b=resource.referenceAt(offset(header,12));
            check(a.allocation==id&&b.allocation==id&&b.offset-a.offset==32,"separate retained packet banks");
        }
        const auto root=resource.referenceAt(offset(GameBodyResources::context(player),0xa4));
        check(same(root,{GameBodyResources::RootsA,player*32,1}),"fixed physical root reference without fake address");
        refuses([&]{resource.knownBuffer(root,32);},"root matrix values remain unknown");
        refuses([&]{resource.knownBuffer(offset(GameBodyResources::context(player),0xac),4);},"CPU pointer metadata not exposed as source NULL");
    }
    auto first=resource.referenceAt(offset(resource.partHeader(0,1),4));
    auto sameSide=resource.referenceAt(offset(resource.partHeader(4,1),4));
    auto otherSide=resource.referenceAt(offset(resource.partHeader(5,1),4));
    resource.knownBuffer(first,24).data[0]=17;
    check(resource.knownBuffer(sameSide,24).data[0]==17&&resource.knownBuffer(otherSide,24).data[0]==0x5a,"native mutation follows same-side alias only");
    GameBodyResources copy(resource);copy.knownBuffer(first,24).data[0]=29;
    check(resource.knownBuffer(first,24).data[0]==17&&copy.knownBuffer(sameSide,24).data[0]==29,"copy isolates allocation while retaining aliases");
    check(same(copy.partHeader(9,19),resource.partHeader(9,19)),"copy retains all reference identities");
    auto copyView=copy.buffer(GameBodyResources::Contexts);copyView.cells[0xb0/4].reference.offset+=4;
    check(!same(copy.partHeader(0,0),resource.partHeader(0,0)),"reference cells are deeply copied too");
    copy=resource;check(same(copy.partHeader(0,0),resource.partHeader(0,0)),"copy assignment restores a consistent generation");
    refuses([&]{resource.referenceAt(GameBodyResources::context(0));},"raw context scalar is not a reference");
    refuses([&]{resource.partHeader(0,20);},"part bounds");
    refuses([&]{resource.partHeader(10,0);},"physical player bounds");
}
void failures(){
    Body home,away;
    auto result=prepareGameBodyResources(contexts(),home.input,away.input,0);
    check(result.result==NBA97_BODY_JOURNAL_LIMIT&&!result.resource&&result.journal.empty(),"zero journal publishes no resource");
    auto complete=prepareGameBodyResources(contexts(),home.input,away.input);
    const auto firstWrites=complete.side[0].writes;
    result=prepareGameBodyResources(contexts(),home.input,away.input,firstWrites+7);
    check(result.result==NBA97_BODY_JOURNAL_LIMIT&&!result.resource&&result.sidesCompleted==1,"away failure does not publish partial owner");
    check(result.journal.size()==firstWrites+7&&result.side[1].writes==7,"home and away failure prefixes retained");
    away.input.known.assign(away.input.bytes.size(),1);away.input.known[4]=0;
    result=prepareGameBodyResources(contexts(),home.input,away.input);
    check(result.result==NBA97_BODY_UNKNOWN&&!result.resource&&result.sidesCompleted==1&&result.journal.size()==firstWrites,"unknown away count does not invent zero");
    away=Body{};home.input.originalAddressMod4=-1;
    result=prepareGameBodyResources(contexts(),home.input,away.input);
    check(result.result==NBA97_BODY_ALIGNMENT_UNKNOWN&&!result.resource,"unknown original alignment refused");
    home=Body{};auto c=contexts();c.known.assign(c.bytes.size(),0);
    result=prepareGameBodyResources(c,home.input,away.input);
    check(result.resource&&result.result==NBA97_BODY_OK,"unread context bytes can remain unknown");
    refuses([&]{result.resource->knownBuffer(offset(GameBodyResources::context(0),4),4);},"normalization does not establish world position");
    c.known[1]=2;result=prepareGameBodyResources(c,home.input,away.input);
    check(result.result==NBA97_BODY_ARGUMENT&&!result.resource,"noncanonical input ownership metadata rejected");
}
}
int main(){try{ownership();failures();std::cout<<checks<<" native body ownership checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
