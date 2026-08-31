#include "game_body_names.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nba97;
using Ref=Nba97GameBodyReference;
unsigned checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(std::to_string(checks)+": "+why);}
void word(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));}
Ref plus(Ref r,std::uint32_t n){r.offset+=n;return r;}
bool same(Ref a,Ref b){return a.known==b.known&&a.allocation==b.allocation&&a.offset==b.offset;}
GameBodyBytes sourceBody(){
    GameBodyBytes input{std::vector<std::uint8_t>(40000,0x5a),{},0};
    auto& b=input.bytes;word(b,0,3);word(b,4,3);std::uint32_t c=12;
    for(unsigned player=0;player<5;++player){
        for(unsigned part=0;part<20;++part){
            const auto n=part==9?5u:1u;c+=12;word(b,c,n);c+=16;
            if(!player)c+=24*n;c+=64*n;
        }
        c+=4;word(b,c,1);word(b,c+4,1);c+=60+24+4;
        const auto ia=c;c+=12;const auto ib=c;c+=16;
        const auto pa=c;c+=12;const auto pb=c;c+=12;
        const auto sa=c;c+=12;const auto sb=c;c+=16;
        for(unsigned i=0;i<3;++i){word(b,ia+i*4,i);word(b,ib+i*4,i);
            word(b,pa+i*4,0x01000008+i*8);word(b,pb+i*4,0x13000008+i*8);
            word(b,sa+i*4,0x01000008+i*8);word(b,sb+i*4,0xff000008+i*8);}
        c+=64+4+64+4;const auto groups=c;c+=100;
        for(unsigned group=0;group<6;++group){word(b,groups+group*16,1);c+=68;}
    }
    b.resize(c);return input;
}
GameBodyResources body(){
    auto result=prepareGameBodyResources({std::vector<std::uint8_t>(30200),{},0},sourceBody(),sourceBody());
    check(result.result==NBA97_BODY_OK&&result.resource,"synthetic resources use actual50768 twice");
    for(unsigned p=0;p<10;++p)for(unsigned bank=0;bank<2;++bank){
        const auto base=result.resource->referenceAt(plus(result.resource->partHeader(p,9),8+bank*4));
        for(unsigned packet=0;packet<2;++packet){auto view=result.resource->knownBuffer(plus(base,packet?0x80:0x40),32);
            view.data[12]=packet?155:56;view.data[28]=packet?56:155;}
    }
    return std::move(*result.resource);
}
struct Textures {
    GameRenderBackend backend;
    Nba97GameRenderTextures state{};
    std::array<Nba97GameRenderPlayer,10> players{};
    Textures(){
        backend.unmaskedTransfersKnown=true;backend.sdkTransferLimitsKnown=true;
        backend.sdkTransferWidth=1024;backend.sdkTransferHeight=512;
        auto retain=[&](std::vector<std::uint8_t> b){const auto n=b.size();const auto id=backend.memory.add(std::move(b),{},0);
            return backend.memory.knownBuffer(id,0,n);};
        std::vector<std::uint8_t> scratch(1516);scratch[0]=0x40;word(scratch,4,100);word(scratch,6,30);
        state.name_scratch={retain(std::move(scratch)),0};
        std::vector<std::uint8_t> glyph(20);glyph[0]=0x40;word(glyph,4,4);word(glyph,6,1);glyph[16]=0x21;glyph[17]=0x43;
        state.glyph[0]={retain(std::move(glyph)),0};
        for(unsigned p=0;p<10;++p){std::vector<std::uint8_t> record(244);record[0x29]='A';record[0x2a]='A';
            players[p].record=retain(std::move(record));state.player[p]=&players[p];state.name_xy[p][0]=32;state.name_xy[p][1]=static_cast<int>(p*32);}
    }
    GameBodyNameRenderResult name(GameBodyResources* b,GameBodyNameState& n,unsigned p){
        return renderGameBodyName(b,n,state,p,GameRenderBackend::renderIo,&backend);
    }
};
void flow(){
    GameBodyNameState names;Textures t;t.state.bypass_name_uv=1;
    // Actual first width calculation needs no body or initialized polygon refs.
    for(unsigned p=0;p<10;++p){const auto r=t.name(nullptr,names,p);
        check(r.entered&&r.result==NBA97_RENDER_COMPLETE&&r.centersWritten==15,"bypass computes all four widths and performs real upload");
        for(unsigned j=0;j<4;++j)check(names.center[p][j].known&&names.center[p][j].word==6&&!names.polygon[p][j].known,"only source center writes become known");
        std::uint16_t pixel=0;check(t.backend.vram.word(32+11,p*32,pixel)&&pixel==0x4321,"actual glyph nibbles reach owned VRAM through946B8");
    }
    auto resources=body();auto adjusted=recenterGameBodyNames(resources,names);
    check(adjusted.result==NBA97_BODY_OK&&adjusted.journal.size()==200&&adjusted.progress.completed,"actual name tail completes against retained resources");
    for(unsigned p=0;p<10;++p)for(unsigned bank=0;bank<2;++bank){
        const auto first=resources.knownBuffer(names.polygon[p][bank*2],29),second=resources.knownBuffer(names.polygon[p][bank*2+1],29);
        check(first.data[12]==99&&first.data[20]==99&&first.data[28]==110,"tail keeps saved old width for packet0");
        check(second.data[12]==110&&second.data[20]==110&&second.data[28]==99,"tail reuses same old width for packet1");
        check(names.center[p][bank*2].word==105&&names.center[p][bank*2+1].word==105,"negative odd endpoint difference floors");
    }
    t.state.bypass_name_uv=0;
    auto copy=resources;auto copyNames=names;
    const auto saved=resources.knownBuffer(names.polygon[0][0],29).data[12];
    auto r=t.name(&copy,copyNames,0);
    check(r.result==NBA97_RENDER_COMPLETE&&r.centersWritten==15,"post-load name redraw executes actual texture and upload owners");
    check(t.state.name_polygon[0][0].data==copy.knownBuffer(copyNames.polygon[0][0],29).data,"borrowed packet view rebuilt into clone");
    t.state.name_polygon[0][0].data[12]=47;
    check(copy.knownBuffer(copyNames.polygon[0][0],29).data[12]==47&&resources.knownBuffer(names.polygon[0][0],29).data[12]==saved,"copy rebound writes do not leak to original");
    // Aliases remain the same retained bytes through both source pairs.
    names.polygon[0][2]=names.polygon[0][0];names.polygon[0][3]=names.polygon[0][1];
    r=t.name(&resources,names,0);
    check(r.result==NBA97_RENDER_COMPLETE&&t.state.name_polygon[0][2].data==t.state.name_polygon[0][0].data,"later pair aliases earlier packet");
    check(names.center[0][0].word==105&&names.center[0][2].word==105,"later pair reads earlier UV writes");
}
int refuse(void*,const Nba97GameRenderIoEvent*){return 0;}
int throwing(void*,const Nba97GameRenderIoEvent*){throw std::runtime_error("native I/O exception");}
void prefixes(){
    auto resources=body();GameBodyNameState names;Textures t;
    auto tail=recenterGameBodyNames(resources,names);
    check(tail.result==NBA97_BODY_UNKNOWN&&tail.progress.stopped_pc==0x80050624&&tail.journal.size()==2,"unknown initial width retains two pointer stores only");
    check(names.polygon[0][0].known&&names.polygon[0][1].known&&!names.center[0][0].known,"unknown width never becomes zero");
    names={};for(auto& row:names.center)for(auto& value:row)value={7,1};
    tail=recenterGameBodyNames(resources,names,13);
    check(tail.result==NBA97_BODY_JOURNAL_LIMIT&&tail.journal.size()==13&&tail.progress.banks_completed==1,"limited tail keeps original first-bank prefix");
    check(names.center[0][2].word==105&&names.center[0][3].word==7,"center store receipt follows exact second-bank prefix");
    GameBodyNameState unknown;t.state.bypass_name_uv=1;
    t.state.name_scratch.storage.size=12;auto r=t.name(nullptr,unknown,0);
    check(r.entered&&r.result==NBA97_RENDER_RESOURCE&&!r.centersWritten&&!unknown.center[0][0].known,"early scratch refusal does not publish fabricated widths");
    t.state.name_scratch.storage.size=1516;t.players[0].record.size=0x29;r=t.name(nullptr,unknown,0);
    check(r.entered&&r.result==NBA97_RENDER_RESOURCE&&!r.centersWritten&&!unknown.center[0][0].known,"early player refusal leaves widths unknown");
    t.players[0].record.size=244;
    r=renderGameBodyName(nullptr,unknown,t.state,0,refuse,nullptr);
    check(r.entered&&r.result==NBA97_RENDER_IO_REFUSED&&r.centersWritten==15&&unknown.center[0][3].known,"upload refusal keeps computed widths");
    unknown={};r=renderGameBodyName(nullptr,unknown,t.state,0,throwing,nullptr);
    check(r.result==NBA97_RENDER_IO_REFUSED&&r.centersWritten==15&&unknown.center[0][0].known&&!r.detail.empty(),"native callback exception preserves original write prefix");
    unknown.center[0][3].known=2;const auto bytes=t.state.name_scratch.storage.data[16];r=t.name(nullptr,unknown,0);
    check(!r.entered&&r.result==NBA97_RENDER_ARGUMENT&&t.state.name_scratch.storage.data[16]==bytes,"noncanonical center metadata refused before C entry");
    // Exercise the C receipt where only the first pair's center stores execute.
    t.state.bypass_name_uv=0;std::array<std::uint8_t,32> a{},b{};
    t.state.name_polygon[0][0]={a.data(),a.size()};t.state.name_polygon[0][1]={b.data(),b.size()};
    t.state.name_polygon[0][2]={};t.state.name_polygon[0][3]={};std::uint8_t mask=0;
    check(nba97_game_render_name_tracked(&t.state,0,refuse,nullptr,&mask)==NBA97_RENDER_RESOURCE&&mask==3,"tracked C reports only executed first-pair center stores");
    auto clean=body();GameBodyNameState valid;for(auto& row:valid.center)for(auto& c:row)c={6,1};
    check(recenterGameBodyNames(clean,valid).result==NBA97_BODY_OK,"valid fixture recentered");
    const auto packet=valid.polygon[0][0];auto view=clean.buffer(packet.allocation);view.known[packet.offset]=0;
    r=t.name(&clean,valid,0);check(!r.entered&&r.result==NBA97_RENDER_RESOURCE,"unknown packet bytes cannot enter legacy raw consumer");
    view.known[packet.offset]=1;view.cells[packet.offset/4].is_reference=1;view.cells[packet.offset/4].reference={GameBodyResources::Home,0,1};
    r=t.name(&clean,valid,0);check(!r.entered&&r.result==NBA97_RENDER_RESOURCE,"pointer sidecar cannot leak raw zero address bytes");
    t.state.bypass_name_uv=1;r=t.name(nullptr,valid,0);
    check(r.result==NBA97_RENDER_COMPLETE,"bypass does not validate unused geometry");
    check(same(valid.polygon[0][0],packet),"width generation never changes polygon identities");
}
}
int main(){try{flow();prefixes();std::cout<<checks<<" body-name binding and upload checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
