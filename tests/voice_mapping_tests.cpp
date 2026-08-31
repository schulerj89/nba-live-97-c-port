#include "recovered/voice_mapping.h"
#include "recovered/voice_programs.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
namespace {
unsigned checks;
void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"mapping check %u failed\n",checks);std::abort();}}
constexpr uint32_t Base=0x80000000,Map=0x80130000,Body=0x80140000;
struct Fixture {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(0x200000),known=std::vector<uint8_t>(bytes.size(),1);
    uint8_t table_bytes[48],table_known[48];
    Nba97VoicePatlSpan span{bytes.data(),known.data(),bytes.size(),Base,1,1,0};
    Nba97VoiceMapping owner{{&span,1},io,this,10000};
    Nba97VoiceMappingTable table{table_bytes,table_known,sizeof table_bytes};Nba97VoiceMappingProgress progress{};
    std::vector<Nba97VoiceMappingEvent> events;unsigned allocations=0,transfers=0,polls=0,waits=0;
    unsigned fail_allocate=0,refuse=0,delay=0;bool mutate=false,invalidate_table=false;
    Fixture(){
        std::memset(table_bytes,0xa5,sizeof table_bytes);std::memset(table_known,1,sizeof table_known);tput(0,0xffffffffu);
        put(Map+6,1,1);put(Map+0x1c,0);put(Map+0x20,64);put(Map+0x24,16);put(Map+0x28,16);put(Map+0x2c,0xffffffffu);put(Map+0x30,0xffffffffu);
        put(0x800c75e8,1);put(0x800c75f0,8);put(0x800c75f4,7);put(0x800c75ec,3);put(0x800c7678,0x55);put(0x800e45e4,255,1);
    }
    void put(uint32_t a,uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i)bytes[a-Base+i]=uint8_t(v>>(8*i));}
    uint32_t get(uint32_t a,unsigned n=4){uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(bytes[a-Base+i])<<(8*i);return v;}
    void tput(unsigned a,uint32_t v){for(unsigned i=0;i<4;++i)table_bytes[a+i]=uint8_t(v>>(8*i));}
    uint32_t tget(unsigned a){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(table_bytes[a+i])<<(8*i);return v;}
    static int io(void* user,const Nba97VoicePatlMemory* memory,const Nba97VoiceMappingEvent* e,uint32_t* result){
        auto& f=*static_cast<Fixture*>(user);check(memory==&f.owner.memory);f.events.push_back(*e);if(f.refuse==f.events.size())return 0;*result=0;
        switch(e->call){
        case NBA97_MAPPING_ALLOCATE_7EC2C:++f.allocations;*result=f.allocations==f.fail_allocate?0xffffffffu:0x10000u+f.allocations*0x1000u;break;
        case NBA97_MAPPING_TRANSFER_7DC90:++f.transfers;*result=0xffffffffu; // Original7EA04 ignores this raw result.
            if(f.invalidate_table){f.table_known[4]=0;f.table_known[7]=2;}
            if(f.mutate)f.put(Map+0x24,e->a1+16);break;
        case NBA97_MAPPING_TEST_EVENT_7F568:++f.polls;*result=f.polls>f.delay;break;
        case NBA97_MAPPING_FREE_7E56C:*result=0xffffffffu;if(f.mutate)f.put(Map+6,1,1);break;
        case NBA97_MAPPING_STREAM_RESET_7390C:f.put(0x800e460c,0x20000);f.put(0x800e4624,0x4000);break;
        case NBA97_MAPPING_STREAM_PRIME_73580:break;
        case NBA97_MAPPING_WAIT_CHANNEL:++f.waits;f.put(0x800c6d2c,0,1);break;
        }
        return 1; // Fixture boundary effects, never evidence of hardware success.
    }
    Nba97VoiceApiResult upload(uint32_t body=Body){return nba97_voice_mapping_upload(&owner,Map,body,&table,&progress);}
};
struct Chain {
    Fixture f;Nba97VoiceHandles handles{};uint32_t banks[10]{};
    Nba97VoicePrograms programs{&handles,banks,program_call,this};
    Nba97VoicePatlUpload patl{{&f.span,1},patl_call,this};
    static constexpr uint32_t Header=Map-56,Bank=0x80120000;
    uint32_t output=0xabcdef01;int inner=1;unsigned forwarded=0;
    Chain(unsigned count=1){
        banks[0]=Bank;f.put(Bank+4,1,1);f.put(Bank+6,1,2);
        f.put(Header,0x6c544150);f.put(Header+5,0,1);f.put(Header+7,count,1);f.put(Header+12,4);
        for(unsigned i=0;i<count;++i){auto t=Header+16+i*92;f.put(t+36,0);
            f.put(t+40+6,1,1);f.put(t+40+0x1c,i*64);f.put(t+40+0x20,i*64+32);
            f.put(t+40+0x24,16);f.put(t+40+0x28,16);f.put(t+40+0x2c,0xffffffff);f.put(t+40+0x30,0xffffffff);}
        f.table.size=24;std::memset(f.table_bytes,0xa5,48);std::memset(f.table_known,0,48);
    }
    static int program_call(void* user,Nba97VoiceProgramCall op,Nba97VoiceProgramRequest* q,uint32_t* value){
        auto& c=*static_cast<Chain*>(user);
        if(op==NBA97_PROGRAM_WRITE32){*value=0;return nba97_voice_patl_write(&c.patl.memory,q->argument[0],4,q->argument[1])==1;}
        if(op<NBA97_PROGRAM_WRITE32)return nba97_voice_patl_read(&c.patl.memory,q->argument[0],op==NBA97_PROGRAM_READ8?1:op==NBA97_PROGRAM_READ16?2:4,value)==1;
        check(op==NBA97_PROGRAM_UPLOAD_PATL_924B4&&q->mapping_table==&c.f.table&&q->argument[2]==0);
        auto r=nba97_voice_patl_upload(&c.patl,q->argument[0],q->argument[1],q->mapping_table);
        if(r.completion!=1)return 0;*value=uint32_t(r.value);return 1;
    }
    static int patl_call(void* user,const Nba97VoicePatlMemory* memory,Nba97VoicePatlCall op,uint32_t mapping,uint32_t body,Nba97VoiceMappingTable* table,uint32_t* value){
        auto& c=*static_cast<Chain*>(user);check(memory==&c.patl.memory);Nba97VoiceApiResult r;
        if(op==NBA97_PATL_UPLOAD_MAPPING_70884){check(table==&c.f.table);++c.forwarded;r=nba97_voice_mapping_upload(&c.f.owner,mapping,body,table,&c.f.progress);}
        else{check(table==nullptr&&body==0);r=nba97_voice_mapping_unload(&c.f.owner,mapping,&c.f.progress);}
        c.inner=r.completion;if(r.completion!=1)return 0;*value=uint32_t(r.value);return 1;
    }
    Nba97VoiceApiResult run(){return nba97_voice_program_register(&programs,0,&output,Header,Body,&f.table);}
    void second_sentinel(){check(nba97_voice_mapping_table_write(&f.table,12,0xffffffff)==1);}
};
void chain_tests(){
    {Chain c;auto r=c.run();check(r.completion==1&&r.value==8&&c.output==0&&c.f.get(Chain::Bank+8)==Chain::Header);
        check(c.forwarded==1&&c.f.get(Chain::Header+5,1)==1&&c.f.tget(0)==0&&c.f.tget(4)==0x11000);
        check(c.f.table_known[8]==0&&c.f.table_known[12]==0&&c.f.table_bytes[12]==0xa5);}
    {Chain c(2);auto r=c.run();check(r.completion==NBA97_PROGRAM_IO_REFUSED&&c.inner==NBA97_PATL_RESOURCE);
        check(c.f.progress.stopped_in_table&&c.f.progress.stopped_address==12&&c.f.allocations==1&&c.forwarded==2);
        check(c.output==0xabcdef01&&c.f.get(Chain::Bank+8)==0&&c.f.get(Chain::Header+5,1)==0&&c.f.get(Map+0x2c)==0x11000);}
    {Chain c(2);c.second_sentinel();auto r=c.run();check(r.completion==1&&r.value==8&&c.forwarded==2&&c.f.allocations==2);
        check(c.f.tget(12)==64&&c.f.tget(16)==0x12000&&c.f.table_known[20]==0&&c.f.table_bytes[24]==0xa5);}
    {Chain c(3);c.second_sentinel();auto r=c.run();check(r.completion==NBA97_PROGRAM_IO_REFUSED&&c.inner==NBA97_PATL_RESOURCE);
        check(c.f.progress.stopped_address==24&&c.forwarded==3&&c.f.allocations==2&&c.output==0xabcdef01&&c.f.table_bytes[24]==0xa5);}
    {Chain c;c.f.fail_allocate=1;auto r=c.run();check(r.completion==1&&r.value==-1&&c.output==0xffffffff&&c.f.get(Chain::Bank+8)==0);
        check(c.f.get(Chain::Header+5,1)==1&&c.f.allocations==1&&c.f.tget(0)==0xffffffff); // Original first-tone failure still sets loaded.
    }
    {Chain c(2);c.second_sentinel();c.f.fail_allocate=2;auto r=c.run();check(r.completion==1&&r.value==-1&&c.output==0xffffffff);
        check(c.f.get(Chain::Header+5,1)==0&&c.f.events.back().call==NBA97_MAPPING_FREE_7E56C&&c.f.events.back().a0==0x11000);
        check(c.f.get(Map+0x2c)==0x11000&&c.f.tget(4)==0x11000); // Original free result ignored; address/table are retained.
    }
    {Fixture f;f.table_known[20]=2;uint32_t word=0;check(nba97_voice_mapping_table_read(&f.table,0,&word)==1&&word==0xffffffff&&f.table_known[20]==2);
        f.table_known[0]=0;f.table_known[3]=2;check(nba97_voice_mapping_table_write(&f.table,0,7)==NBA97_VOICE_TABLE_METADATA&&f.tget(0)==0xffffffff&&f.table_known[0]==0);}
}

}
int main(){
    chain_tests();
    {Fixture f;auto r=nba97_voice_mapping_upload(&f.owner,0,Body,nullptr,&f.progress);check(r.completion==1&&r.value==-8&&f.events.empty());}
    {Fixture f;auto r=f.upload();check(r.completion==1&&r.value==0);check(f.get(Map+0x2c)==0x11000);check(f.get(Map+0x1c)==0);check(f.get(0x800c75c4,2)==0x2200);
        check(f.tget(0)==0&&f.tget(4)==0x11000&&f.tget(8)==0xa5a5a5a5&&f.tget(12)==0xa5a5a5a5);check(f.get(0x800f9600)==1&&f.get(0x800c75f8)==1&&f.get(0x800c6d2d,1)==0);}
    {Fixture f;f.put(Map+6,2,1);auto r=f.upload();check(r.completion==1&&r.value==0&&f.transfers==2);check(f.tget(12)==64&&f.tget(16)==0x12000&&f.tget(24)==0xa5a5a5a5);}
    {Fixture f;f.put(Map+6,2,1);f.put(Map+0x28,32);auto r=f.upload();check(r.completion==1&&r.value==-1&&f.allocations==2&&f.transfers==2);
        check(f.get(Map+0x2c)==0x11000&&f.get(Map+0x30)==0xffffffffu);check(f.tget(12)==0xa5a5a5a5&&f.get(0x800c6d2d,1)==0);}
    {Fixture f;f.fail_allocate=1;auto r=f.upload();check(r.completion==1&&r.value==-1&&f.events.size()==1);check(f.get(0x800c6d2d,1)==0&&f.tget(0)==0xffffffffu);}
    {Fixture f;f.refuse=2;auto r=f.upload();check(r.completion==NBA97_PATL_IO_REFUSED&&f.allocations==1);check(f.get(0x800c6d2d,1)==1&&f.get(0x800f9600)==1);check(f.get(Map+0x2c)==0xffffffffu);}
    {Fixture f;f.put(Map+0x24,0x7f001);auto r=f.upload();check(r.completion==1&&r.value==-1);check(f.events[1].a1==0x7f000&&f.get(Map+0x2c)==0xffffffffu);}
    {Fixture f;f.delay=3;auto r=f.upload();check(r.completion==1&&r.value==0&&f.polls==4);}
    {Fixture f;f.delay=1000;f.owner.step_budget=35;auto r=f.upload();check(r.completion==NBA97_MAPPING_LIMIT&&f.get(0x800c6d2d,1)==1&&f.get(Map+0x2c)==0xffffffffu);}
    {Fixture f;f.put(0x800c6d2c,1,1);auto r=f.upload();check(r.completion==1&&f.waits==1&&f.events.front().call==NBA97_MAPPING_WAIT_CHANNEL);}
    {Fixture f;f.put(0x800c75f0,0);auto r=f.upload();check(r.completion==NBA97_MAPPING_TRAP&&f.allocations==1&&f.transfers==0&&f.get(0x800c6d2d,1)==1);}
    {Fixture f;f.tput(0,0);f.tput(4,0x77770);f.tput(12,0xffffffffu);auto r=f.upload();check(r.completion==1&&r.value==0&&f.allocations==0);check(f.get(Map+0x1c)==0xffffffffu&&f.get(Map+0x2c)==0x77770);}
    {Fixture f;f.put(Map+6,2,1);f.tput(0,0);f.tput(4,0x77770);f.tput(12,64);f.tput(16,0x88880);f.tput(24,0xffffffffu);auto r=f.upload();check(r.completion==1&&f.events.empty());
        check(f.get(Map+0x20)==0xffffffffu&&f.get(Map+0x30)==0x88880);r=nba97_voice_mapping_unload(&f.owner,Map,&f.progress);check(r.completion==1&&r.value==0&&f.events.empty());}
    {Fixture f;f.table_known[0]=0;f.table_known[3]=2;auto r=f.upload();check(r.completion==NBA97_PATL_METADATA&&f.progress.stopped_in_table==1&&f.get(0x800c6d2d,1)==1);}
    {Fixture f;std::memset(f.table_known+4,0,44);auto r=f.upload();check(r.completion==1&&f.table_known[4]==1&&f.table_known[12]==0);r=f.upload();check(r.completion==NBA97_PATL_RESOURCE&&f.progress.stopped_in_table==1&&f.progress.stopped_address==12);}
    {Fixture f;f.table.size=4;auto r=f.upload();check(r.completion==NBA97_PATL_RESOURCE&&f.progress.stopped_address==4);check(f.get(Map+0x2c)==0x11000&&f.tget(0)==0&&f.get(0x800c6d2d,1)==1);}
    {Fixture f;f.table_known[4]=2;auto r=f.upload();check(r.completion==NBA97_PATL_METADATA&&f.tget(0)==0&&f.tget(4)==0xa5a5a5a5);}
    {Fixture f;f.invalidate_table=true;auto r=f.upload();check(r.completion==NBA97_PATL_METADATA&&f.get(Map+0x2c)==0x11000&&f.tget(0)==0);
        check(f.tget(4)==0xa5a5a5a5&&f.table_known[4]==0&&f.table_known[7]==2);}
    {Fixture f;f.mutate=true;auto r=f.upload();check(r.completion==1&&r.value==-1&&f.get(Map+0x24)==32&&f.get(Map+0x2c)==0xffffffffu);}
    {Fixture f;f.put(Map+6,2,1);f.put(Map+0x2c,0x11000);f.put(Map+0x30,0x12000);auto r=nba97_voice_mapping_unload(&f.owner,Map,&f.progress);
        check(r.completion==1&&r.value==0&&f.events.size()==2);check(f.get(Map+0x2c)==0x11000&&f.get(Map+0x30)==0x12000);}
    {Fixture f;f.put(Map+6,2,1);f.mutate=true;auto r=nba97_voice_mapping_unload(&f.owner,Map,&f.progress);check(r.completion==1&&f.events.size()==1);}
    {Fixture f;f.put(Map+6,2,1);auto r=f.upload(0);check(r.completion==1&&r.value==0&&f.events.size()==2);check(f.get(Map+0x2c)==0x20000&&f.get(Map+0x30)==0x24000);}
    {Fixture f;f.put(0x800e45e4,0,1);auto r=f.upload(0);check(r.completion==1&&r.value==0&&f.events.empty()&&f.get(Map+0x2c)==0xffffffffu);}
    std::printf("voice_mapping: %u checks passed\n",checks);return 0;
}
