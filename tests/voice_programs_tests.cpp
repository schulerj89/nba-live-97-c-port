#include "recovered/voice_programs.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
unsigned checks;
void check(bool b){++checks;if(!b)throw std::runtime_error("voice program check "+std::to_string(checks));}
struct Fixture {
    static constexpr uint32_t base=0x80120000,program=base+1024,body=base+2048;
    std::array<uint8_t,4096> memory{};uint32_t banks[10]{};Nba97VoiceHandles handles{};
    Nba97VoicePrograms state{};uint32_t output=0xabcdef01,backend_result=0;
    std::array<uint8_t,24> table_bytes{},table_known{};
    Nba97VoiceMappingTable table{table_bytes.data(),table_known.data(),table_bytes.size()};
    uint32_t captured_first_word=0;
    std::vector<Nba97VoiceProgramCall> calls;Nba97VoiceProgramRequest captured{};
    bool refuse_backend=false,mutate_bank=false,refuse_write=false,refuse_tag=false;
    Fixture(){table_bytes.fill(0xa5);handles.enabled=1;banks[0]=base;state={&handles,banks,invoke,this};put(base+4,1,1);put(base+6,3,2);put(program,0x6c544150,4);}
    void put(uint32_t address,uint32_t value,unsigned size){for(unsigned i=0;i<size;++i)memory.at(address-base+i)=static_cast<uint8_t>(value>>(8*i));}
    uint32_t get(uint32_t address,unsigned size){uint32_t n=0;for(unsigned i=0;i<size;++i)n|=uint32_t(memory.at(address-base+i))<<(8*i);return n;}
    static int invoke(void* p,Nba97VoiceProgramCall op,Nba97VoiceProgramRequest* request,uint32_t* result){
        auto& f=*static_cast<Fixture*>(p);f.calls.push_back(op);
        if(op<=NBA97_PROGRAM_WRITE32){
            auto address=request->argument[0];unsigned size=op==NBA97_PROGRAM_READ8?1:op==NBA97_PROGRAM_READ16?2:4;
            if(f.refuse_tag&&address==program)return 0;
            if(address<base||uint64_t(address-base)+size>f.memory.size())return 0;
            if(op==NBA97_PROGRAM_WRITE32){if(f.refuse_write)return 0;f.put(address,request->argument[1],size);*result=0;}
            else *result=f.get(address,size);return 1;
        }
        f.captured=*request;if(f.refuse_backend)return 0;
        if(f.mutate_bank){f.banks[0]=base+256;f.put(base+8,0xaaaaaaaa,4);}
        if(request->mapping_table){
            check(request->mapping_table==&f.table);
            check(nba97_voice_mapping_table_read(request->mapping_table,0,&f.captured_first_word)==1);
            check(nba97_voice_mapping_table_write(request->mapping_table,0,123)==1);
        }
        *result=f.backend_result;return 1;
    }
};
void play(){
    Fixture f;auto r=nba97_voice_program_play(&f.state,10,0,127);check(r.completion==1&&r.value==-8&&f.calls.empty());
    f.handles.enabled=0;r=nba97_voice_program_play(&f.state,0,0,127);check(r.value==-10&&f.calls.empty());f.handles.enabled=1;
    r=nba97_voice_program_play(&f.state,0,0xffffffff,127);check(r.value==-8&&f.calls.size()==2);
    r=nba97_voice_program_play(&f.state,0,3,127);check(r.value==-8);
    r=nba97_voice_program_play(&f.state,0,0,127);check(r.value==-8); // Null program is source invalid input.
    f.put(Fixture::base+8,Fixture::program,4);f.backend_result=37;
    r=nba97_voice_program_play(&f.state,0,0,108);check(r.value==37&&f.calls.back()==NBA97_PROGRAM_PLAY_PATL_9267C);
    check(f.captured.argument[0]==Fixture::program&&f.captured.argument[1]==0&&f.captured.argument[6]==108);
    check(f.captured.argument[2]==0xffffffff&&f.captured.argument[3]==0xffffffff&&f.captured.argument[4]==0xffffffff&&f.captured.argument[5]==0xffffffff&&f.captured.argument[7]==0xffffffff);
    f.put(Fixture::program,0x5450,4);f.backend_result=0xfffffff7;
    r=nba97_voice_program_play(&f.state,0,0,255);check(r.value==-9&&f.calls.back()==NBA97_PROGRAM_PLAY_PT_91CD8&&f.captured.argument[6]==255);
    f.put(Fixture::program,0,4);r=nba97_voice_program_play(&f.state,0,0,127);check(r.value==-7);
    f.put(Fixture::program,0x6c544150,4);f.refuse_backend=true;
    r=nba97_voice_program_play(&f.state,0,0,127);check(r.completion==NBA97_PROGRAM_IO_REFUSED);
    f.banks[0]=1;r=nba97_voice_program_play(&f.state,0,0,127);check(r.completion==NBA97_PROGRAM_IO_REFUSED);
}
void registration(){
    Fixture f;f.handles.enabled=0;f.put(Fixture::base+8,0x1234,4);f.backend_result=99;
    auto r=nba97_voice_program_register(&f.state,0,&f.output,Fixture::program,Fixture::body,&f.table);
    check(r.value==8&&f.output==1&&f.get(Fixture::base+12,4)==Fixture::program); // Enabled is not checked here.
    check(f.captured_first_word==0xffffffff&&f.captured.mapping_table==&f.table&&f.captured.argument[2]==0);
    Fixture fail;fail.backend_result=0xfffffff5;
    r=nba97_voice_program_register(&fail.state,0,&fail.output,Fixture::program,Fixture::body,&fail.table);
    check(r.value==-11&&fail.output==0xffffffff&&!fail.get(Fixture::base+8,4));
    Fixture unknown;unknown.put(Fixture::program,0,4);
    r=nba97_voice_program_register(&unknown.state,0,&unknown.output,Fixture::program,Fixture::body,&unknown.table);
    check(r.value==-1&&unknown.output==0xffffffff);
    Fixture full;for(unsigned i=0;i<3;++i)full.put(Fixture::base+8+i*4,1,4);
    r=nba97_voice_program_register(&full.state,0,&full.output,Fixture::program,Fixture::body,nullptr);
    check(r.value==-9&&full.output==0xabcdef01);
    Fixture mutate;mutate.mutate_bank=true;r=nba97_voice_program_register(&mutate.state,0,&mutate.output,Fixture::program,Fixture::body,&mutate.table);
    check(r.value==8&&mutate.output==0&&mutate.banks[0]==Fixture::base+256&&mutate.get(Fixture::base+8,4)==Fixture::program);
    Fixture refused;refused.refuse_write=true;r=nba97_voice_program_register(&refused.state,0,&refused.output,Fixture::program,Fixture::body,&refused.table);
    check(r.completion==NBA97_PROGRAM_IO_REFUSED&&refused.output==0xabcdef01&&!refused.get(Fixture::base+8,4));
    Fixture legacy;legacy.put(Fixture::base+4,0,1);legacy.put(Fixture::base+6,0,2);
    for(unsigned i=0;i<127;++i)legacy.put(Fixture::base+8+i*4,1,4);
    r=nba97_voice_program_register(&legacy.state,0,&legacy.output,Fixture::program,Fixture::body,&legacy.table);
    check(r.value==8&&legacy.output==127&&legacy.get(Fixture::base+8+127*4,4)==Fixture::program);
    r=nba97_voice_bank_validate(&legacy.state,0xffffffff);check(r.value==-8);
    check(nba97_voice_program_play(nullptr,0,0,0).completion==NBA97_PROGRAM_ARGUMENT);
}
void table_order(){
    Fixture invalid;auto r=nba97_voice_program_register(&invalid.state,10,&invalid.output,Fixture::program,Fixture::body,nullptr);
    check(r.value==-8&&invalid.calls.empty());
    Fixture zero;zero.put(Fixture::base+6,0,2);zero.table.size=25;
    r=nba97_voice_program_register(&zero.state,0,&zero.output,Fixture::program,Fixture::body,&zero.table);
    check(r.value==-9&&zero.table_bytes[0]==0xa5&&zero.table_known[0]==0);
    Fixture oversized;oversized.table.size=25;
    r=nba97_voice_program_register(&oversized.state,0,&oversized.output,Fixture::program,Fixture::body,&oversized.table);
    check(r.completion==NBA97_PROGRAM_TABLE_METADATA&&oversized.calls.size()==3&&oversized.table_bytes[0]==0xa5);
    Fixture malformed;malformed.table_known[3]=2;
    r=nba97_voice_program_register(&malformed.state,0,&malformed.output,Fixture::program,Fixture::body,&malformed.table);
    check(r.completion==NBA97_PROGRAM_TABLE_METADATA&&malformed.calls.size()==3&&malformed.table_bytes[0]==0xa5&&malformed.table_known[0]==0);
    Fixture refused;refused.refuse_tag=true;
    r=nba97_voice_program_register(&refused.state,0,&refused.output,Fixture::program,Fixture::body,&refused.table);
    uint32_t word=0;check(r.completion==NBA97_PROGRAM_IO_REFUSED&&nba97_voice_mapping_table_read(&refused.table,0,&word)==1&&word==0xffffffff);
    check(refused.output==0xabcdef01&&refused.table_bytes[4]==0xa5&&refused.table_known[4]==0);
    Fixture alias;alias.table.data=alias.memory.data()+1024;
    r=nba97_voice_program_register(&alias.state,0,&alias.output,Fixture::program,Fixture::body,&alias.table);
    check(r.completion==1&&r.value==-1&&alias.output==0xffffffff&&alias.get(Fixture::program,4)==0xffffffff);
    check(alias.calls.size()==5&&alias.calls.back()==NBA97_PROGRAM_READ16); // Sentinel precedes both tag reads.
}
}
int main(){try{play();registration();table_order();std::cout<<checks<<" source bank/play/upload dispatch checks passed\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
