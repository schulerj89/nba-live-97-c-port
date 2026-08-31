#include "game_court_frame_compose.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {
using Word=std::uint32_t;
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
constexpr Word Origin=0x80000000u,Root=0x80110000u,Vertices=0x80140000u;
constexpr Word Tex=Root+32u,Flat=Root+512u;
Word xy(int x,int y){return (Word(x)&65535u)|(Word(y)<<16);}
unsigned widthMask(unsigned width){return (1u<<width)-1u;}
Word valueMask(unsigned width){return width==4?UINT32_MAX:(1u<<(width*8))-1u;}

struct Event {
    Word pc,address,value;unsigned width,kind;
    bool operator==(const Event& other)const{
        return pc==other.pc&&address==other.address&&value==other.value&&
               width==other.width&&kind==other.kind;
    }
};

struct Memory {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000,0xa5);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x200000,1);
    std::unordered_map<Word,Nba97GameBodyReference> references;
    std::vector<Event> events;
    Word failure_pc{};int failure_status{NBA97_BODY_OK};
    Word malformed_reference_pc{};
    unsigned reference_reads{};

    void put(Word address,unsigned width,Word word){
        for(unsigned i=0;i<width;++i)bytes.at(address-Origin+i)=std::uint8_t(word>>(i*8));
    }
    Word get(Word address,unsigned width=4)const{
        Word word=0;for(unsigned i=0;i<width;++i)word|=Word(bytes.at(address-Origin+i))<<(i*8);return word;
    }
    bool span(Word address,unsigned width)const{
        return address>=Origin&&Word(address-Origin)<=bytes.size()&&width<=bytes.size()-(address-Origin);
    }
    static int frame(void* user,Word pc,Word address,unsigned width,unsigned kind,Nba97PlayerFrameValue* value){
        if(!user||!value)return NBA97_BODY_ARGUMENT;
        auto& memory=*static_cast<Memory*>(user);
        if(pc==memory.failure_pc)return memory.failure_status;
        if(!memory.span(address,width))return NBA97_BODY_BOUNDS;
        if(kind==NBA97_FRAME_READ){
            *value={};
            for(unsigned i=0;i<width;++i)if(memory.known[address-Origin+i]){
                value->word|=Word(memory.bytes[address-Origin+i])<<(i*8);
                value->known_mask|=std::uint8_t(1u<<i);
            }
            const auto found=memory.references.find(address);
            if(width==4&&found!=memory.references.end()){
                value->is_reference=1;value->reference=found->second;++memory.reference_reads;
            }
            if(pc==memory.malformed_reference_pc){
                value->is_reference=1;value->reference={0,address-Origin,1};value->known_mask=1;value->word&=255;
            }
            memory.events.push_back({pc,address,value->word,width,kind});return NBA97_BODY_OK;
        }
        if(kind!=NBA97_FRAME_WRITE&&kind!=NBA97_FRAME_WRITE_POINTER)return NBA97_BODY_ARGUMENT;
        const unsigned mask=widthMask(width);
        if(value->is_reference||value->known_mask!=mask)return NBA97_BODY_ARGUMENT;
        for(unsigned i=0;i<width;++i){memory.bytes[address-Origin+i]=std::uint8_t(value->word>>(i*8));memory.known[address-Origin+i]=1;}
        const Word cell=address&~3u;memory.references.erase(cell);
        if(kind==NBA97_FRAME_WRITE_POINTER&&width==4&&memory.span(value->word,0))
            memory.references[address]={0,value->word-Origin,1};
        memory.events.push_back({pc,address,value->word&valueMask(width),width,kind});return NBA97_BODY_OK;
    }
    static int direct(void* user,Word pc,Word address,unsigned width,int write,Nba97CourtValue* value){
        if(!user||!value)return NBA97_COURT_ARGUMENT;
        auto& memory=*static_cast<Memory*>(user);
        if(!memory.span(address,width))return NBA97_COURT_RESOURCE;
        if(write){
            for(unsigned i=0;i<width;++i){memory.bytes[address-Origin+i]=std::uint8_t(value->word>>(i*8));memory.known[address-Origin+i]=1;}
            memory.events.push_back({pc,address,value->word&valueMask(width),width,NBA97_FRAME_WRITE});
        }else{
            for(unsigned i=0;i<width;++i)if(!memory.known[address-Origin+i]){*value={0,0};return NBA97_COURT_COMPLETE;}
            *value={memory.get(address,width),1};memory.events.push_back({pc,address,value->word,width,NBA97_FRAME_READ});
        }
        return NBA97_COURT_COMPLETE;
    }
};

void seedMemory(Memory& memory){
    memory.put(0x800febe4,4,Root);memory.put(Root,4,11);memory.put(Root+4,4,1);
    memory.put(Root+8,4,Vertices);memory.put(Root+12,4,Tex);memory.put(Root+16,4,Flat);
    memory.put(0x800dcf10,4,0);memory.put(0x8001ede8,4,0);
    memory.put(0x801046d8,4,0x80150000);memory.put(0x80102924,4,0x80160000);
    memory.put(0x800febec,4,0);memory.put(0x800fcc54,4,0xffffffff);
    const std::array<std::int16_t,9> rotation={4096,0,0,0,4096,0,0,0,4096};
    for(unsigned i=0;i<5;++i){
        const Word low=std::uint16_t(rotation[i*2]);
        const Word high=i==4?0:Word(std::uint16_t(rotation[i*2+1]))<<16;
        memory.put(0x800f9fd8+i*4,4,low|high);
    }
    memory.put(0x800f9fec,4,0);memory.put(0x800f9ff0,4,0);memory.put(0x800f9ff4,4,1024);
    for(unsigned group=0;group<12;++group){
        const Word record=group<11?Tex+group*16:Flat;
        memory.put(record,4,group);memory.put(record+4,4,1);
        memory.put(record+8,4,0x80130000+group*128);memory.put(record+12,4,0x80130040+group*128);
        const std::array<Word,4> point={xy(-128,-128),xy(128,-128),xy(128,128),xy(-128,128)};
        for(unsigned i=0;i<4;++i){memory.put(Vertices+group*32+i*8,4,point[i]);memory.put(Vertices+group*32+i*8+4,4,0);}
    }
    memory.put(0x800feda0,4,0x80112000);memory.put(0x80112000,4,0x80112100);
}

Nba97GamePeriodValue known(Word word){return {word,1};}
void seed(nba97::GameNetGeometry& geometry){
    const std::array<std::int16_t,9> rotation={4096,0,0,0,4096,0,0,0,4096};
    for(unsigned i=0;i<5;++i)geometry.player.root.vector.rotation[i]=known(
        Word(std::uint16_t(rotation[i*2]))|(i==4?0:Word(std::uint16_t(rotation[i*2+1]))<<16));
    geometry.player.root.vector.translation={known(0),known(0),known(1024)};
    geometry.player.root.offset_x=known(256u<<16);geometry.player.root.offset_y=known(120u<<16);
    geometry.player.root.distance=known(128);geometry.player.root.depth_cue_a=known(0);geometry.player.root.depth_cue_b=known(0);
    geometry.average_scale4=known(1024);
}
void seed(nba97::GameCourtGeometry& geometry){
    geometry.camera.rotation={4096,0,0,0,4096,0,0,0,4096};geometry.camera.translation={0,0,1024};
    geometry.camera.offset_x=256<<16;geometry.camera.offset_y=120<<16;geometry.camera.distance=128;
    geometry.camera.depth_cue_a=0;geometry.camera.depth_cue_b=0;geometry.camera.average_scale4=1024;geometry.camera.known=true;
    geometry.leading_bits={32,1};
}
bool same(Nba97CourtValue a,Nba97GamePeriodValue b){return a.word==b.word&&a.known==b.known;}
void compareGeometry(const nba97::GameCourtGeometry& direct,const nba97::GameNetGeometry& composed){
    for(unsigned i=0;i<6;++i)require(same(direct.vertex[i],i<2?composed.player.root.vector.vertex[i]:composed.player.extra_vertex[i-2]),"vertex state matches direct owner");
    for(unsigned i=0;i<3;++i){
        require(same(direct.screen[i],composed.player.root.screen[i]),"screen FIFO matches direct owner");
        require(same(direct.mac[i+1],composed.player.root.vector.mac[i]),"MAC state matches direct owner");
        require(same(direct.ir[i+1],composed.player.root.vector.ir[i]),"IR state matches direct owner");
        require(Word(direct.camera.translation[i])==composed.player.root.vector.translation[i].word,"translation matches direct owner");
    }
    for(unsigned i=0;i<4;++i)require(same(direct.depth[i],composed.player.root.depth[i]),"depth FIFO matches direct owner");
    require(same(direct.mac[0],composed.player.root.mac0)&&same(direct.ir[0],composed.player.root.ir0),"MAC0 and IR0 match direct owner");
    require(same(direct.flags,composed.player.root.vector.flags)&&same(direct.order_depth,composed.player.order_depth),"FLAG and OTZ match direct owner");
    for(unsigned i=0;i<9;++i){const Word packed=composed.player.root.vector.rotation[i/2].word>>((i&1)*16);require(std::uint16_t(direct.camera.rotation[i])==std::uint16_t(packed),"rotation matches direct owner");}
}
bool sameProgress(const Nba97CourtProgress& a,const Nba97CourtProgress& b){
    return a.operations==b.operations&&a.reads==b.reads&&a.stores==b.stores&&
        a.math_operations==b.math_operations&&a.quads==b.quads&&a.linked==b.linked&&
        a.stopped_pc==b.stopped_pc&&a.stopped_address==b.stopped_address&&a.completed==b.completed;
}
struct Direct {
    Memory* memory{};nba97::GameCourtGeometry* geometry{};
    static int access(void* user,Word pc,Word address,unsigned width,int write,Nba97CourtValue* value){
        return Memory::direct(static_cast<Direct*>(user)->memory,pc,address,width,write,value);
    }
    static int math(void* user,const Nba97CourtMathRequest* request,Nba97CourtValue* value){
        return static_cast<Direct*>(user)->geometry->apply(*request,*value);
    }
};

nba97::GameCourtFrameCompose owner(Memory& memory){
    nba97::GameCourtFrameCompose result;seed(result.geometry);result.leading_bits=known(32);
    result.memory={Memory::frame,nullptr,nullptr,&memory,0};return result;
}
void directComparison(){
    Memory adapted_memory,direct_memory;seedMemory(adapted_memory);seedMemory(direct_memory);
    auto adapted=owner(adapted_memory);nba97::GameCourtGeometry geometry;seed(geometry);
    Direct direct{&direct_memory,&geometry};Nba97CourtContext context{Direct::access,Direct::math,&direct,100000};
    Nba97CourtProgress adapted_progress{},direct_progress{};
    const int direct_status=nba97_game_court_frame(&context,&direct_progress);
    const int adapted_status=adapted.run(100000,adapted_progress);
    if(direct_status!=NBA97_COURT_COMPLETE||adapted_status!=NBA97_BODY_OK)
        std::cerr<<"direct="<<direct_status<<" adapted="<<adapted_status<<" stopped="<<std::hex
                 <<adapted_progress.stopped_pc<<'/'<<adapted_progress.stopped_address<<std::dec
                 <<" screen="<<unsigned(adapted.geometry.player.root.screen[0].known)<<','
                 <<unsigned(adapted.geometry.player.root.screen[1].known)<<','
                 <<unsigned(adapted.geometry.player.root.screen[2].known)<<'\n';
    require(direct_status==NBA97_COURT_COMPLETE,"direct 4AC68 owner completes");
    require(adapted_status==NBA97_BODY_OK,"frame-context adapter completes");
    require(sameProgress(adapted_progress,direct_progress),"adapter progress exactly matches direct owner");
    require(adapted_memory.bytes==direct_memory.bytes&&adapted_memory.known==direct_memory.known,"adapter memory exactly matches direct owner");
    require(adapted_memory.events.size()==direct_memory.events.size(),"adapter and direct event counts match");
    for(std::size_t i=0;i<adapted_memory.events.size();++i){
        const auto& a=adapted_memory.events[i];const auto& d=direct_memory.events[i];
        require(a.pc==d.pc&&a.address==d.address&&a.width==d.width&&a.value==d.value,"ordered adapter event matches direct owner");
        require((a.kind==NBA97_FRAME_READ)==(d.kind==NBA97_FRAME_READ),"adapter event direction matches direct owner");
    }
    const std::array<Word,5> pointer_pc={0x8004acdc,0x8004ace4,0x8004ade4,0x8004af0c,0x8004b0ec};
    for(Word pc:pointer_pc){bool found=false;for(const auto& event:adapted_memory.events)if(event.pc==pc){found=true;require(event.kind==NBA97_FRAME_WRITE_POINTER,"cursor publication uses normalized pointer write");}require(found,"all cursor publications reached");}
    for(const auto& event:adapted_memory.events)if(event.width==3&&event.kind!=NBA97_FRAME_READ)require(event.kind==NBA97_FRAME_WRITE,"low24 tag remains scalar");
    require(adapted_memory.reference_reads>0,"published normalized cursors are read through retained reference identity");
    compareGeometry(geometry,adapted.geometry);
    require(adapted_progress.quads==12&&adapted_progress.linked==13,"complete court composition coverage");
}

void refusalAndStatusCases(){
    Memory limited;seedMemory(limited);auto limited_owner=owner(limited);Nba97CourtProgress progress{};
    require(limited_owner.run(0,progress)==NBA97_BODY_JOURNAL_LIMIT,"court operation limit explicitly maps to body journal limit");
    require(progress.stopped_pc==0x80055f18&&progress.operations==0,"zero budget stops before first access");

    Memory alignment;seedMemory(alignment);alignment.put(0x800febe4,4,Root+2);auto alignment_owner=owner(alignment);
    require(alignment_owner.run(100000,progress)==NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT,"intrinsic court alignment explicitly maps to projection alignment refusal");
    require(progress.stopped_pc==0x8004acbc&&progress.stopped_address==Root+10,"alignment refusal identifies original access");

    for(int status:{NBA97_BODY_BOUNDS,NBA97_BODY_ALIGNMENT_UNKNOWN,NBA97_BODY_ALIGNMENT_TRAP,
                    NBA97_BODY_REFERENCE_REQUIRED,NBA97_BODY_ADDRESS_REQUIRED,NBA97_BODY_JOURNAL_LIMIT}){
        Memory failed;seedMemory(failed);failed.failure_pc=0x80055f18;failed.failure_status=status;auto failed_owner=owner(failed);
        require(failed_owner.run(100000,progress)==status,"borrowed frame status survives court result namespace");
        require(progress.stopped_pc==0x80055f18&&progress.operations==1,"borrowed refusal retains exact source prefix");
    }

    Memory unknown;seedMemory(unknown);unknown.known[0x800f9fe8-Origin]=0;auto unknown_owner=owner(unknown);
    require(unknown_owner.run(100000,progress)==NBA97_BODY_UNKNOWN,"partial frame knownness becomes court unknown");
    require(progress.stopped_pc==0x80055f28&&progress.math_operations==0&&progress.stores==0,"unknown matrix byte refuses before matrix mutation");

    Memory malformed;seedMemory(malformed);malformed.malformed_reference_pc=0x80055f18;auto malformed_owner=owner(malformed);
    require(malformed_owner.run(100000,progress)==NBA97_BODY_ARGUMENT,"known reference with partial bytes is malformed");
    require(progress.stopped_pc==0x80055f18,"malformed reference refuses at source read");

    Memory control;seedMemory(control);auto control_owner=owner(control);control_owner.geometry.player.root.offset_x={};
    require(control_owner.run(100000,progress)==NBA97_BODY_UNKNOWN,"unknown projection control is not invented");
    require(progress.stopped_pc==0x80054db0&&progress.stores==2,"unknown control retains camera-load and cursor-store prefix");

    Memory leading;seedMemory(leading);auto leading_owner=owner(leading);leading_owner.leading_bits={};
    require(leading_owner.run(100000,progress)==NBA97_BODY_UNKNOWN,"unknown LZCR remains independent retained state");
    require(progress.stopped_pc==0x80054de4,"unknown LZCR refuses at original MFC2 data31");
    leading_owner=owner(leading);leading_owner.leading_bits=known(33);
    require(leading_owner.run(100000,progress)==NBA97_BODY_ARGUMENT,"LZCR count outside hardware domain refuses");

    nba97::GameCourtFrameCompose missing;require(missing.run(100,progress)==NBA97_BODY_ARGUMENT,"missing borrowed memory refuses");
}
}

int main(){
    try{directComparison();refusalAndStatusCases();std::cout<<checks<<" court-frame compose checks passed\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
