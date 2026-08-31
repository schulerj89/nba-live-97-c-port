#include "recovered/game_net_transform.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Word=std::uint32_t;
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
constexpr Word Origin=0x80000000u;
struct Event {
    Word pc,address,value;unsigned width,kind;
    bool operator==(const Event& other)const{return pc==other.pc&&address==other.address&&value==other.value&&width==other.width&&kind==other.kind;}
};
struct Memory {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x100000,0xa5);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x100000,1);
    std::vector<Event> events;
    Word failure_pc{};int failure_status{NBA97_BODY_OK};Word malformed_pc{};
    Word mutation_pc{},mutation_address{},mutation_value{};
    void put(Word address,Word word,unsigned width=4){for(unsigned i=0;i<width;++i)bytes.at(address-Origin+i)=std::uint8_t(word>>(8*i));}
    Word get(Word address,unsigned width=4)const{Word word=0;for(unsigned i=0;i<width;++i)word|=Word(bytes.at(address-Origin+i))<<(8*i);return word;}
    static int access(void* user,Word pc,Word address,unsigned width,unsigned kind,Nba97PlayerFrameValue* value){
        if(!user||!value)return NBA97_BODY_ARGUMENT;
        auto& memory=*static_cast<Memory*>(user);
        if(pc==memory.failure_pc)return memory.failure_status;
        if(address<Origin||address-Origin>memory.bytes.size()||width>memory.bytes.size()-(address-Origin))return NBA97_BODY_BOUNDS;
        if(kind==NBA97_FRAME_READ){
            *value={};for(unsigned i=0;i<width;++i)if(memory.known[address-Origin+i]){
                value->word|=Word(memory.bytes[address-Origin+i])<<(8*i);value->known_mask|=std::uint8_t(1u<<i);
            }
            if(pc==memory.malformed_pc){value->is_reference=1;value->reference={0,address-Origin,1};value->known_mask=1;value->word&=255;}
        }else if(kind==NBA97_FRAME_WRITE){
            for(unsigned i=0;i<width;++i){memory.bytes[address-Origin+i]=std::uint8_t(value->word>>(8*i));memory.known[address-Origin+i]=1;}
        }else return NBA97_BODY_ARGUMENT;
        memory.events.push_back({pc,address,value->word,width,kind});
        if(pc==memory.mutation_pc)memory.put(memory.mutation_address,memory.mutation_value);
        return NBA97_BODY_OK;
    }
};
void seed(Memory& memory,Word x=0x00000100,Word scale=8,Word z=0xfffffe00,Word y=0x00000300){
    memory.put(0x800fdba0,0);memory.put(0x800fc9a0,x);memory.put(0x800b2044,scale);
    memory.put(0x800fc9b0,0x100);memory.put(0x800fc9ac,0x900);
    memory.put(0x800fc9a8,z);memory.put(0x800fc9a4,y);
}
int run(Memory& memory,std::size_t budget,Nba97PlayerFrameProgress& progress){
    Nba97PlayerFrameContext context{Memory::access,nullptr,nullptr,&memory,budget};
    return nba97_game_net_transform(&context,&progress);
}
std::uint16_t position(Word source,Word scale){
    const Word low=Word(std::uint64_t(source)*scale);
    const std::int64_t signed_low=low<0x80000000u?low:std::int64_t(low)-0x100000000ll;
    return std::uint16_t(-signed_low/8);
}
void nominalAndOrder(){
    Memory memory;seed(memory);Nba97PlayerFrameProgress progress{};
    require(run(memory,100,progress)==NBA97_BODY_OK&&progress.completed,"active transform completes");
    require(progress.operations==16&&progress.reads==7&&progress.stores==9,"exact active operation totals");
    require(memory.get(0x800fa63c,2)==0&&memory.get(0x800fa630,2)==0&&memory.get(0x800fa632,2)==0&&memory.get(0x800fa634,2)==0,"four source zero half-stores");
    require(memory.get(0x800fa638,2)==0x700&&memory.get(0x800fa63a,2)==0xff00,"source angle subtraction truncates to halfwords");
    require(memory.get(0x800fab98,2)==position(0x100,8),"X low32 product");
    require(memory.get(0x800fab9a,2)==position(0xfffffe00,8),"negative Z adds seven before shift");
    require(memory.get(0x800fab9c,2)==position(0x300,8),"Y low32 product");
    const std::vector<Event> expected={
        {0x8002dc8c,0x800fdba0,0,4,NBA97_FRAME_READ},
        {0x8002dca0,0x800fc9a0,0x100,4,NBA97_FRAME_READ},
        {0x8002dca8,0x800b2044,8,4,NBA97_FRAME_READ},
        {0x8002dcb0,0x800fc9b0,0x100,4,NBA97_FRAME_READ},
        {0x8002dcb8,0x800fc9ac,0x900,4,NBA97_FRAME_READ},
        {0x8002dcc4,0x800fa63c,0,2,NBA97_FRAME_WRITE},
        {0x8002dccc,0x800fa630,0,2,NBA97_FRAME_WRITE},
        {0x8002dcd4,0x800fa632,0,2,NBA97_FRAME_WRITE},
        {0x8002dcdc,0x800fa634,0,2,NBA97_FRAME_WRITE},
        {0x8002dcf0,0x800fa638,0x700,2,NBA97_FRAME_WRITE},
        {0x8002dcf8,0x800fa63a,0xff00,2,NBA97_FRAME_WRITE},
        {0x8002dd10,0x800fc9a8,0xfffffe00,4,NBA97_FRAME_READ},
        {0x8002dd28,0x800fab98,0xff00,2,NBA97_FRAME_WRITE},
        {0x8002dd40,0x800fc9a4,0x300,4,NBA97_FRAME_READ},
        {0x8002dd58,0x800fab9a,0x200,2,NBA97_FRAME_WRITE},
        {0x8002dd78,0x800fab9c,0xfd00,2,NBA97_FRAME_WRITE}};
    require(memory.events==expected,"exact original read and half-store order");
}
void gateAndLiveReads(){
    Memory gated;seed(gated);gated.put(0x800fdba0,1);const auto before=gated.bytes;Nba97PlayerFrameProgress progress{};
    require(run(gated,1,progress)==NBA97_BODY_OK&&progress.completed,"nonzero FDBA0 gate completes");
    require(progress.operations==1&&progress.reads==1&&progress.stores==0&&gated.events.size()==1,"gate performs only its live read");
    require(gated.bytes==before,"gate leaves all destinations untouched");

    Memory second;seed(second);second.mutation_pc=0x8002dcc4;second.mutation_address=0x800fc9a8;second.mutation_value=0x400;
    require(run(second,100,progress)==NBA97_BODY_OK&&second.get(0x800fab9a,2)==position(0x400,8),"FC9A8 is read after initial stores");
    Memory third;seed(third);third.mutation_pc=0x8002dd28;third.mutation_address=0x800fc9a4;third.mutation_value=0xfffffc00;
    require(run(third,100,progress)==NBA97_BODY_OK&&third.get(0x800fab9c,2)==position(0xfffffc00,8),"FC9A4 is read after FAB98 store");
}
void arithmeticCases(){
    const std::array<Word,10> values={0,1,7,8,0x7fffffffu,0x80000000u,0x80000001u,0xfffffff8u,0xffffffffu,0x55555555u};
    Nba97PlayerFrameProgress progress{};
    for(Word scale:values)for(Word x:values){
        Memory memory;seed(memory,x,scale,x^0xa5a5a5a5u,x+0x10203040u);
        require(run(memory,100,progress)==NBA97_BODY_OK,"edge arithmetic transform completes");
        require(memory.get(0x800fab98,2)==position(x,scale),"X edge low32/truncation arithmetic");
        require(memory.get(0x800fab9a,2)==position(x^0xa5a5a5a5u,scale),"Z edge low32/truncation arithmetic");
        require(memory.get(0x800fab9c,2)==position(x+0x10203040u,scale),"Y edge low32/truncation arithmetic");
    }
    Word state=0x2dc88;
    for(unsigned i=0;i<500;++i){
        state=state*1664525u+1013904223u;const Word x=state;
        state=state*1664525u+1013904223u;const Word z=state;
        state=state*1664525u+1013904223u;const Word y=state;
        state=state*1664525u+1013904223u;const Word scale=state;
        Memory memory;seed(memory,x,scale,z,y);require(run(memory,100,progress)==NBA97_BODY_OK,"random arithmetic transform completes");
        require(memory.get(0x800fab98,2)==position(x,scale)&&memory.get(0x800fab9a,2)==position(z,scale)&&memory.get(0x800fab9c,2)==position(y,scale),"random exact low32 products");
    }
}
void refusalPrefixes(){
    Memory baseline;seed(baseline);Nba97PlayerFrameProgress complete{};require(run(baseline,100,complete)==NBA97_BODY_OK,"prefix baseline");
    for(std::size_t budget=0;budget<baseline.events.size();++budget){
        Memory stopped;seed(stopped);Nba97PlayerFrameProgress progress{};
        require(run(stopped,budget,progress)==NBA97_BODY_JOURNAL_LIMIT,"every native budget boundary refuses");
        require(progress.operations==budget&&!progress.completed&&stopped.events.size()==budget,"budget retains exact access prefix");
        for(std::size_t i=0;i<budget;++i)require(stopped.events[i]==baseline.events[i],"budget event prefix matches complete run");
    }
    for(std::size_t index=0;index<baseline.events.size();++index)if(baseline.events[index].kind==NBA97_FRAME_READ){
        Memory unknown;seed(unknown);unknown.known[baseline.events[index].address-Origin]=0;Nba97PlayerFrameProgress progress{};
        require(run(unknown,100,progress)==NBA97_BODY_UNKNOWN,"unknown reached source refuses");
        require(progress.stopped_pc==baseline.events[index].pc&&progress.stopped_address==baseline.events[index].address,"unknown refusal source location");
        require(unknown.events.size()==index+1,"unknown read retains exact earlier access prefix");
    }
    for(int status:{NBA97_BODY_BOUNDS,NBA97_BODY_ALIGNMENT_UNKNOWN,NBA97_BODY_ALIGNMENT_TRAP,NBA97_BODY_REFERENCE_REQUIRED,NBA97_BODY_ADDRESS_REQUIRED,NBA97_BODY_JOURNAL_LIMIT}){
        Memory failed;seed(failed);failed.failure_pc=0x8002dca8;failed.failure_status=status;Nba97PlayerFrameProgress progress{};
        require(run(failed,100,progress)==status,"borrowed memory status passes through unchanged");
        require(progress.operations==3&&failed.events.size()==2&&progress.stopped_pc==0x8002dca8,"callback refusal retains exact prefix");
    }
    Memory malformed;seed(malformed);malformed.malformed_pc=0x8002dc8c;Nba97PlayerFrameProgress progress{};
    require(run(malformed,100,progress)==NBA97_BODY_ARGUMENT&&progress.stopped_pc==0x8002dc8c,"malformed normalized reference refuses");
    require(nba97_game_net_transform(nullptr,&progress)==NBA97_BODY_ARGUMENT,"missing context refuses");
    Nba97PlayerFrameContext missing{};require(nba97_game_net_transform(&missing,&progress)==NBA97_BODY_ARGUMENT,"missing access refuses");
    require(nba97_game_net_transform(&missing,nullptr)==NBA97_BODY_ARGUMENT,"missing progress refuses");
}
}
int main(){try{nominalAndOrder();gateAndLiveReads();arithmeticCases();refusalPrefixes();std::cout<<checks<<" net-transform checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
