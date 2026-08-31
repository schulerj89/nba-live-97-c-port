#include "recovered/voice_patl_upload.h"
#include "recovered/voice_programs.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
unsigned checks;
void check(bool b){++checks;if(!b)throw std::runtime_error("PATl upload check "+std::to_string(checks));}
struct Fixture {
    static constexpr uint32_t base=0x80130000,body=base+2048;
    std::array<uint8_t,4096> data{},known{};
    Nba97VoicePatlSpan span{};Nba97VoicePatlUpload state{};
    std::array<uint8_t,24> table_bytes{},table_known{};
    Nba97VoiceMappingTable table{table_bytes.data(),table_known.data(),24};
    uint32_t uploads=0,unloads=0,fail_upload=0,refuse_upload=0;
    bool shorten=false,move_cleanup=false,corrupt_metadata=false;
    std::vector<uint32_t> events;
    Fixture(unsigned count=3){
        table_known.fill(1);table_bytes.fill(0xa5);nba97_voice_mapping_table_write(&table,0,0xffffffffu);
        known.fill(1);span={data.data(),known.data(),data.size(),base,1,1,0};state={{&span,1},invoke,this};
        put(0,0x6c544150,4);put(7,count,1);put(12,4,4);
        for(unsigned block:{16u,512u})for(unsigned i=0;i<8;++i){
            auto t=block+i*92;put(t+24,4,4);put(t+28,0,4);put(t+32,8,4);put(t+36,0,4);put(t+40,0x6c784d54,4);
        }
        put(2048,0x3412,2);
    }
    void put(std::size_t at,uint32_t value,unsigned width){for(unsigned i=0;i<width;++i)data.at(at+i)=uint8_t(value>>(8*i));}
    uint32_t get(std::size_t at,unsigned width=4){uint32_t v=0;for(unsigned i=0;i<width;++i)v|=uint32_t(data.at(at+i))<<(8*i);return v;}
    static int invoke(void* p,const Nba97VoicePatlMemory* memory,Nba97VoicePatlCall op,uint32_t mapping,uint32_t body_token,Nba97VoiceMappingTable* table,uint32_t* result){
        auto& f=*static_cast<Fixture*>(p);f.events.push_back(mapping);uint32_t value;
        if(op==NBA97_PATL_UPLOAD_MAPPING_70884){
            check(table==&f.table);
            ++f.uploads;if(f.refuse_upload==f.uploads)return 0;
            if(nba97_voice_patl_read(memory,mapping,4,&value)!=1||value!=0x6c784d54||nba97_voice_patl_read(memory,body_token,2,&value)!=1)return 0;
            if(nba97_voice_mapping_table_write(table,0,value)!=1||nba97_voice_mapping_table_write(table,12,f.uploads)!=1)return 0;
            *result=f.fail_upload==f.uploads?0xfffffff5:f.uploads;
            if(f.shorten)f.put(7,0,1);
            if(f.corrupt_metadata)f.span.writable=2;
        }else{
            check(table==nullptr&&body_token==0);
            ++f.unloads;*result=0xfffffff1; // Source ignores this original result.
            if(f.move_cleanup&&f.unloads==1)f.put(12,base+512,4);
            if(f.shorten)f.put(7,0,1);
        }
        return 1;
    }
    Nba97VoiceApiResult upload(){return nba97_voice_patl_upload(&state,base,body,&table);}
};
void normal(){
    Fixture f;auto r=f.upload();check(r.completion==1&&r.value==3&&f.uploads==3&&!f.unloads);
    uint32_t word=0;check(f.get(12)==Fixture::base+16&&f.get(5,1)==1&&nba97_voice_mapping_table_read(&f.table,0,&word)==1&&word==0x3412);
    check(nba97_voice_mapping_table_read(&f.table,12,&word)==1&&word==3&&f.table_bytes[8]==0xa5);
    check(f.get(16+24)==Fixture::base+44&&f.get(16+28)==0&&f.get(16+32)==Fixture::base+56&&f.get(16+36)==Fixture::base+52);
    r=f.upload();check(r.value==-1&&f.uploads==3); // No double relocation when loaded.
    r=nba97_voice_patl_unload(&f.state,Fixture::base);check(r.value==0&&f.unloads==3&&f.get(5,1)==1);
    Fixture zero(0);r=zero.upload();check(r.value==0&&!zero.uploads&&zero.get(12)==Fixture::base+16&&zero.get(5,1)==1);
    Fixture unused(0);r=nba97_voice_patl_upload(&unused.state,Fixture::base,Fixture::body,nullptr);check(r.completion==1&&r.value==0&&unused.get(5,1)==1);
    r=nba97_voice_patl_upload(&unused.state,Fixture::base,Fixture::body,nullptr);check(r.completion==1&&r.value==-1);
}
void failures(){
    Fixture first;first.fail_upload=1;auto r=first.upload();
    check(r.value==-11&&first.get(5,1)==1&&first.uploads==1&&!first.unloads&&first.get(12)==Fixture::base+16);
    r=first.upload();check(r.value==-1&&first.uploads==1); // Original first failure still marks loaded.
    Fixture later;later.fail_upload=3;later.move_cleanup=true;r=later.upload();
    check(r.value==-11&&!later.get(5,1)&&later.uploads==3&&later.unloads==2);
    check(later.events[3]==Fixture::base+56&&later.events[4]==Fixture::base+512+92+40);
    Fixture refused;refused.refuse_upload=1;r=refused.upload();
    check(r.completion==NBA97_PATL_IO_REFUSED&&!refused.get(5,1)&&refused.get(12)==Fixture::base+16&&!refused.unloads);
    Fixture body;body.known[2048]=0;r=body.upload();check(r.completion==NBA97_PATL_IO_REFUSED&&!body.get(5,1));
    Fixture shorten;shorten.shorten=true;r=shorten.upload();check(r.value==1&&shorten.uploads==1&&shorten.get(5,1)==1);
    Fixture unload;unload.put(12,Fixture::base+16,4);unload.shorten=true;
    r=nba97_voice_patl_unload(&unload.state,Fixture::base);check(r.value==0&&unload.unloads==1);
}
void knownness(){
    Fixture f;f.known[5]=0;auto r=f.upload();check(r.completion==NBA97_PATL_RESOURCE&&f.get(12)==4&&!f.uploads);
    f.known[5]=1;f.known[7]=0;r=f.upload();check(r.completion==NBA97_PATL_RESOURCE&&f.get(12)==4);
    f.known[7]=1;f.known[16+24]=0;r=f.upload();check(r.completion==NBA97_PATL_RESOURCE&&f.get(12)==Fixture::base+16&&!f.uploads);
    f.span.writable=0;check(nba97_voice_patl_write(&f.state.memory,Fixture::base+3000,4,0)==NBA97_PATL_RESOURCE);
    f.span.writable=1;for(unsigned i=3000;i<3004;++i)f.known[i]=0;
    check(nba97_voice_patl_write(&f.state.memory,Fixture::base+3000,4,0x12345678)==1&&f.known[3003]==1);
    uint32_t value=0;check(nba97_voice_patl_read(&f.state.memory,Fixture::base+3000,4,&value)==1&&value==0x12345678);
    f.span.source_address_known=0;check(nba97_voice_patl_read(&f.state.memory,Fixture::base,4,&value)==NBA97_PATL_RESOURCE);
    f.span.source_address_known=1;check(nba97_voice_patl_read(&f.state.memory,Fixture::base+1,4,&value)==NBA97_PATL_RESOURCE);
    Nba97VoicePatlSpan aliases[2]={f.span,f.span};aliases[1].source_address=0x80140000;
    Nba97VoicePatlMemory memory{aliases,2};
    check(nba97_voice_patl_write(&memory,0x80140000+3000,4,77)==1&&f.get(3000)==77);
    check(nba97_voice_patl_read(&memory,Fixture::base+3000,4,&value)==1&&value==77);
    aliases[1].source_address=Fixture::base;check(nba97_voice_patl_write(&memory,Fixture::base+3000,4,88)==NBA97_PATL_RESOURCE&&f.get(3000)==77);
    aliases[0].source_address=0xfffffff0;memory.count=1;check(nba97_voice_patl_read(&memory,0xfffffff0,4,&value)==NBA97_PATL_RESOURCE);
    check(nba97_voice_patl_upload(nullptr,0,0,nullptr).completion==NBA97_PATL_ARGUMENT);
}
void metadata(){
    Fixture f;uint32_t value=0xabcdef01;
    f.known[0]=0;f.known[3]=2;f.known[4095]=2;
    check(nba97_voice_patl_read(&f.state.memory,Fixture::base,4,&value)==NBA97_PATL_METADATA&&value==0xabcdef01);
    auto original=f.get(0);
    check(nba97_voice_patl_write(&f.state.memory,Fixture::base,4,0)==NBA97_PATL_METADATA&&f.get(0)==original&&f.known[0]==0&&f.known[3]==2);
    f.known[3]=1;
    check(nba97_voice_patl_read(&f.state.memory,Fixture::base,4,&value)==NBA97_PATL_RESOURCE&&f.known[4095]==2);
    check(nba97_voice_patl_write(&f.state.memory,Fixture::base,4,0x6c544150)==1&&f.known[0]==1&&f.known[4095]==2);
    f.span.fully_known=1;f.known[5]=0;
    check(nba97_voice_patl_read(&f.state.memory,Fixture::base+5,1,&value)==NBA97_PATL_METADATA);
    check(nba97_voice_patl_write(&f.state.memory,Fixture::base+5,1,1)==NBA97_PATL_METADATA&&!f.get(5,1));
    f.known[5]=1;
    check(nba97_voice_patl_read(&f.state.memory,Fixture::base+5,1,&value)==1&&value==0&&f.known[4095]==2);
    f.span.source_address_known=2;
    check(nba97_voice_patl_read(&f.state.memory,Fixture::base+5,1,&value)==NBA97_PATL_METADATA);
    f.span.source_address_known=1;f.span.writable=2;
    check(nba97_voice_patl_write(&f.state.memory,Fixture::base+5,1,1)==NBA97_PATL_METADATA&&!f.get(5,1));
    f.span.writable=1;f.span.fully_known=2;
    check(nba97_voice_patl_read(&f.state.memory,Fixture::base+5,1,&value)==NBA97_PATL_METADATA);
    f.span.fully_known=1;f.span.known=nullptr;
    check(nba97_voice_patl_read(&f.state.memory,Fixture::base+5,1,&value)==1&&value==0);
    Fixture callback;callback.corrupt_metadata=true;auto r=callback.upload();
    check(r.completion==NBA97_PATL_METADATA&&callback.uploads==1&&!callback.get(5,1)&&callback.get(12)==Fixture::base+16);
}
struct Composed {
    Fixture fixture{1};Nba97VoiceHandles handles{};uint32_t banks[10]{};Nba97VoicePrograms programs{};
    static constexpr uint32_t bank=Fixture::base+3000;
    Composed(){handles.enabled=1;banks[0]=bank;fixture.put(3004,1,1);fixture.put(3006,1,2);programs={&handles,banks,call,this};}
    static int call(void* p,Nba97VoiceProgramCall op,Nba97VoiceProgramRequest* request,uint32_t* value){
        auto& c=*static_cast<Composed*>(p);auto& f=c.fixture;
        if(op<=NBA97_PROGRAM_WRITE32){
            if(op==NBA97_PROGRAM_WRITE32){*value=0;return nba97_voice_patl_write(&f.state.memory,request->argument[0],4,request->argument[1])==1;}
            const unsigned width=op==NBA97_PROGRAM_READ8?1:op==NBA97_PROGRAM_READ16?2:4;
            return nba97_voice_patl_read(&f.state.memory,request->argument[0],width,value)==1;
        }
        if(op!=NBA97_PROGRAM_UPLOAD_PATL_924B4)return 0;
        auto result=nba97_voice_patl_upload(&f.state,request->argument[0],request->argument[1],request->mapping_table);
        if(result.completion!=1)return 0;*value=uint32_t(result.value);return 1;
    }
};
void composed(){
    Composed c;uint32_t out=0xffffffff;
    auto r=nba97_voice_program_register(&c.programs,0,&out,Fixture::base,Fixture::body,&c.fixture.table);
    check(r.completion==1&&r.value==8&&out==0&&c.fixture.get(3008)==Fixture::base);
    check(c.fixture.get(5,1)==1&&c.fixture.uploads==1&&c.fixture.get(12)==Fixture::base+16);
    Composed failure;failure.fixture.fail_upload=1;r=nba97_voice_program_register(&failure.programs,0,&out,Fixture::base,Fixture::body,&failure.fixture.table);
    check(r.value==-11&&out==0xffffffff&&!failure.fixture.get(3008)&&failure.fixture.get(5,1)==1);
    Composed refused;refused.fixture.refuse_upload=1;out=123;r=nba97_voice_program_register(&refused.programs,0,&out,Fixture::base,Fixture::body,&refused.fixture.table);
    check(r.completion==NBA97_PROGRAM_IO_REFUSED&&out==123&&!refused.fixture.get(3008)&&!refused.fixture.get(5,1));
}
}
int main(){try{normal();failures();knownness();metadata();composed();std::cout<<checks<<" PATl relocation/ownership/failure checks passed\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
