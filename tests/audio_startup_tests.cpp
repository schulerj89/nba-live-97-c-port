#include "recovered/audio_startup.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <vector>
namespace {
unsigned checks;
void check_at(bool ok,int line) { ++checks;if(!ok) { std::fprintf(stderr,"audio startup check %u at %d\n",checks,line);std::exit(1); } }
#define check(v) check_at(bool(v),__LINE__)
constexpr uint32_t Parameter=0x80100000u,Spu=0x1f801c00u;
struct Block {
    uint32_t address;std::vector<uint8_t> bytes,known;
    Block(uint32_t a,size_t n):address(a),bytes(n,0xcd),known(n,0) {}
};
struct Fixture {
    std::vector<Block> blocks;
    std::vector<Nba97VoicePatlSpan> spans;
    Nba97AudioStartup owner{};Nba97AudioStartupProgress progress{};
    std::array<Nba97AudioStartupEvent,256> journal{};
    std::map<uint32_t,uint32_t> device;
    std::function<int(const Nba97AudioStartupEvent&,Nba97SpuTransferValue&)> hook;
    unsigned initialize=0,heap=0;
    Fixture() {
        blocks.emplace_back(Parameter,40);blocks.emplace_back(0x80027e18u,64);
        blocks.emplace_back(0x800c6d28u,40);blocks.emplace_back(0x800c75c8u,4);
        blocks.emplace_back(0x800d96e8u,32);blocks.emplace_back(0x800e45e4u,80);
        for(auto& b:blocks)spans.push_back({b.bytes.data(),b.known.data(),b.bytes.size(),b.address,1,1,0});
        owner={{spans.data(),spans.size()},io,this,10000};
        put(0x800c75c8u,Spu);put(0x800c6d28u,0,1);
        for(uint32_t i=0;i<8;++i) {
            put(0x800d96e8u+i*4u,0);
            put(0x80027e18u+i*4u,i?0x8007df04u+(i-1)*8u:0x8007df3cu);
            put(0x80027e38u+i*4u,i?0x8007dfccu+(i-1)*8u:0x8007e004u);
        }
        device[Spu+0x1aa]=0xc000;
    }
    void put(uint32_t at,uint32_t value,uint32_t width=4) { check(nba97_voice_patl_write(&owner.memory,at,width,value)==1); }
    uint32_t get(uint32_t at,uint32_t width=4) { uint32_t v=0;check(nba97_voice_patl_read(&owner.memory,at,width,&v)==1);return v; }
    void parameters(uint32_t mask) {
        for(uint32_t i=0;i<40;i+=4)put(Parameter+i,0);
        put(Parameter,mask);put(Parameter+4,0x1234,2);put(Parameter+6,0x5678,2);
        put(Parameter+0x10,0x9abc,2);put(Parameter+0x12,0xdef0,2);
        put(Parameter+0x1c,0x123,2);put(Parameter+0x1e,0x456,2);
    }
    static int io(void* p,const Nba97VoicePatlMemory* m,const Nba97AudioStartupEvent* e,Nba97SpuTransferValue* v) {
        auto& f=*static_cast<Fixture*>(p);check(m==&f.owner.memory);
        if(f.hook) { int rc=f.hook(*e,*v);if(rc!=-99)return rc; }
        switch(e->kind) {
        case NBA97_AUDIO_STARTUP_INITIALIZE:
            check(e->address==0x8007e6ecu&&e->argument[0]==0&&e->argument[1]==0);
            ++f.initialize;*v={0xabcdef01,0};return 1;
        case NBA97_AUDIO_STARTUP_HEAP:
            check(e->address==0x8007e940u&&e->argument[0]==128&&e->argument[1]==0x800fee50u);
            ++f.heap;*v={0xffffffffu,1};return 1; // Original caller ignores raw failure bits.
        case NBA97_AUDIO_STARTUP_DEVICE_READ:
            for(const auto& b:f.blocks)if(e->address>=b.address&&e->address-b.address<b.bytes.size())
                return nba97_voice_patl_read(m,e->address,e->width,&v->word)==1?(v->known=1,1):0;
            if(!f.device.count(e->address))return 0;
            *v={f.device[e->address],1};return 1;
        case NBA97_AUDIO_STARTUP_DEVICE_WRITE:
            for(const auto& b:f.blocks)if(e->address>=b.address&&e->address-b.address<b.bytes.size())
                return nba97_voice_patl_write(m,e->address,e->width,e->value)==1;
            f.device[e->address]=e->value;return 1;
        default:return 0;
        }
    }
    int run(Nba97AudioStartupOperation op=NBA97_AUDIO_STARTUP_700B0,uint32_t a0=0,size_t cap=256) {
        return nba97_audio_startup(&owner,op,a0,journal.data(),cap,&progress);
    }
};
void startup_and_reset() {
    Fixture f;check(f.run()==1&&f.progress.completed&&!f.progress.transferred);
    check(f.progress.returned.known&&f.progress.returned.word==0&&f.initialize==1&&f.heap==1);
    check(f.progress.stopped_pc==0&&!f.progress.stopped_local);
    check(f.get(0x800c6d28u,1)==1&&f.get(0x800d96e8u)==0x8007a6a8u);
    for(auto offset:{0x180u,0x182u,0x1b0u,0x1b2u})check(f.device[Spu+offset]==0x3fff);
    check(f.device[Spu+0x1aa]==0xc001&&f.get(0x800e460cu)==0xffffffffu);
    check(f.get(0x800e45e8u,1)==2&&f.get(0x800e45f4u,2)==2&&f.get(0x800e45fcu,2)==0xffff);
    uint32_t v=0;check(nba97_voice_patl_read(&f.owner.memory,0x800e4608u,4,&v)==NBA97_PATL_RESOURCE);
    check(nba97_voice_patl_read(&f.owner.memory,0x800e4620u,4,&v)==NBA97_PATL_RESOURCE);
    check(f.run()==1&&f.get(0x800d96ecu)==0); // Guard avoids duplicate callback only.
    check(f.initialize==2&&f.heap==2); // Other startup work repeats in original.
    std::array<unsigned,40> local{};unsigned stores=0;
    for(size_t i=0;i<f.progress.events;++i)if(f.journal[i].kind==NBA97_AUDIO_STARTUP_PARAMETER_STORE) {
        const auto& e=f.journal[i];++stores;check(e.address+e.width<=40&&e.completed);
        for(uint32_t j=0;j<e.width;++j)++local[e.address+j];
    }
    check(stores==12&&std::count(local.begin(),local.end(),2u)==16);
    check(std::count(local.begin(),local.end(),0u)==24);
    Fixture reset;check(reset.run(NBA97_AUDIO_STARTUP_RESET_73A68)==1);
    check(reset.progress.returned.word==0xffffffffu&&reset.progress.stores==30);
    check(reset.get(0x800c6d28u,1)==0&&!reset.initialize&&!reset.heap&&reset.device.size()==1);
}
void registration_bugs() {
    for(uint32_t vacant=0;vacant<=8;++vacant) {
        Fixture f;for(uint32_t i=0;i<vacant;++i)f.put(0x800d96e8u+i*4u,0x90000000u+i);
        check(f.run(NBA97_AUDIO_STARTUP_REGISTER_8E0E0,0x12345678)==1&&f.progress.returned.word==0);
        check(f.progress.stores==(vacant<8?1u:0u));
        if(vacant<8)check(f.get(0x800d96e8u+vacant*4u)==0x12345678);
    }
    Fixture null;check(null.run(NBA97_AUDIO_STARTUP_REGISTER_8E0E0,0)==1);
    check(null.progress.stores==1&&null.get(0x800d96e8u)==0);
    Fixture full;for(uint32_t i=0;i<8;++i)full.put(0x800d96e8u+i*4u,i+1);
    check(full.run()==1&&full.get(0x800c6d28u,1)==1&&full.get(0x800d96e8u)==1);
    full.put(0x800d96e8u,0);check(full.run()==1&&full.get(0x800d96e8u)==0);
    // Preserved original bug: the guard remains set despite the earlier full table.
}
void volume_modes() {
    for(bool right:{false,true})for(uint32_t mode:{0u,1u,2u,3u,4u,5u,6u,7u,8u,0xffffu,0x8000u})
        for(uint32_t value:{0u,1u,127u,128u,0x7fffu,0x8000u,0xffffu}) {
            Fixture f;f.parameters(right?0xau:5u);f.put(Parameter+(right?10u:8u),mode,2);
            f.put(Parameter+(right?6u:4u),value,2);
            check(f.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameter)==1);
            uint32_t expected=value&0x7fffu;
            if(mode>=1&&mode<=7)expected=0x8000u+(mode-1u)*0x1000u+((value&0x8000u)?0u:std::min(value,127u));
            check(f.device[Spu+(right?0x182u:0x180u)]==expected&&f.progress.returned.word==0);
        }
    for(bool right:{false,true})for(uint32_t mode=0;mode<8;++mode)for(uint32_t target=0;target<8;++target) {
        Fixture f;f.parameters(right?0xau:5u);f.put(Parameter+(right?10u:8u),mode,2);
        uint32_t pc=target?(right?0x8007dfccu:0x8007df04u)+(target-1u)*8u:(right?0x8007e004u:0x8007df3cu);
        f.put((right?0x80027e38u:0x80027e18u)+mode*4u,pc);
        check(f.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameter)==1);
        check(f.device[Spu+(right?0x182u:0x180u)]==(target?0x807fu+(target-1u)*0x1000u:(right?0x5678u:0x1234u)));
    }
    Fixture all;all.parameters(0);check(all.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameter)==1);
    check(all.device[Spu+0x1aa]==0xc000&&all.progress.returned.word==Spu);
    check(all.device[Spu+0x1b0]==0x9abc&&all.device[Spu+0x1b6]==0x456);
    for(uint32_t mask:{0x10u,0x20u,0x4000u,0x80000000u}) {
        Fixture f;f.put(Parameter,mask);check(f.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameter)==1);
        check(f.progress.accesses==1&&!f.progress.events&&f.progress.returned.word==0);
    }
}
void live_mutations() {
    {Fixture f;f.parameters(0x2c3);f.put(Parameter+0x18,1);
        f.hook=[&](const auto& e,auto&) {
            if(e.pc==0x8007df84u) {f.put(Parameter,0x2000);f.put(Parameter+6,0x4321,2);}
            return -99;
        };check(f.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameter)==1);
        check(f.device[Spu+0x182]==0x4321&&f.device[Spu+0x1b0]==0x9abc&&f.progress.returned.word==0);}
    {Fixture f;f.parameters(0x2000);f.put(Parameter+0x24,1);
        f.hook=[&](const auto& e,auto&) {if(e.pc==0x8007e220u)f.put(0x800c75c8u,Spu+0x200);return -99;};
        check(f.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameter)==1);
        check(f.progress.returned.word==Spu&&f.device[Spu+0x1aa]==0xc002&&!f.device.count(Spu+0x3aa));}
    {Fixture f;f.parameters(3);f.put(0x800c75c8u,Parameter-0x180);
        check(f.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameter)==1);
        check(f.get(Parameter,2)==0x1234&&f.get(Parameter+2,2)==0x5678&&f.progress.returned.word==0);}
    {Fixture f;f.parameters(5);f.put(0x80027e18u,0x8007e004u);
        check(f.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameter)==NBA97_PATL_IO_REFUSED);
        check(f.progress.stopped_pc==0x8007defcu&&f.progress.stopped_address==0x8007e004u&&f.progress.events==0);}
}
void prefixes_and_contracts() {
    for(auto op:{NBA97_AUDIO_STARTUP_700B0,NBA97_AUDIO_STARTUP_COMMON_7DEA8,NBA97_AUDIO_STARTUP_RESET_73A68,NBA97_AUDIO_STARTUP_REGISTER_8E0E0}) {
        Fixture full;full.parameters(0);check(full.run(op,Parameter)==1);
        for(size_t budget=0;budget<full.progress.accesses;++budget) {
            Fixture f;f.parameters(0);f.owner.access_budget=budget;
            check(f.run(op,Parameter)==NBA97_AUDIO_STARTUP_LIMIT&&f.progress.accesses==budget&&!f.progress.completed);
        }
        for(size_t cap=0;cap<full.progress.events;++cap) {
            Fixture f;f.parameters(0);check(f.run(op,Parameter,cap)==NBA97_AUDIO_STARTUP_LIMIT&&f.progress.events==cap);
            for(size_t i=0;i<cap;++i)check(f.journal[i].kind==full.journal[i].kind&&f.journal[i].pc==full.journal[i].pc&&
                f.journal[i].address==full.journal[i].address&&f.journal[i].value==full.journal[i].value&&f.journal[i].completed);
        }
    }
    for(auto kind:{NBA97_AUDIO_STARTUP_INITIALIZE,NBA97_AUDIO_STARTUP_HEAP,NBA97_AUDIO_STARTUP_DEVICE_READ,NBA97_AUDIO_STARTUP_DEVICE_WRITE}) {
        for(int result:{0,1,2,3}) {
            Fixture f;f.hook=[=](const auto& e,auto& v) {if(e.kind==kind) {v={0x12345678,result==1?uint8_t(2):uint8_t(0)};return result;}return -99;};
            int rc=f.run();check(rc==(result==1?NBA97_PATL_METADATA:result==2?(kind==NBA97_AUDIO_STARTUP_INITIALIZE?2:NBA97_PATL_METADATA):NBA97_PATL_IO_REFUSED));
            const auto& last=f.journal[f.progress.events-1];check(last.completed==(result==1||result==2));
            check(f.progress.completed==(result==2&&kind==NBA97_AUDIO_STARTUP_INITIALIZE));
            if(f.progress.completed)check(f.heap==0&&f.progress.events==1&&f.progress.transferred);
        }
    }
    {Fixture f;f.hook=[](const auto& e,auto& v) {if(e.kind==NBA97_AUDIO_STARTUP_DEVICE_READ) {v={0xbeef,0};return 1;}return -99;};
        check(f.run()==NBA97_PATL_RESOURCE&&f.journal[f.progress.events-1].completed);}
    {Fixture f;f.blocks[4].known[0]=0;check(f.run()==NBA97_PATL_RESOURCE&&f.get(0x800c6d28u,1)==1);}
    {Fixture f;f.owner.access_budget=2;check(f.run()==NBA97_AUDIO_STARTUP_LIMIT&&f.progress.stopped_local&&f.progress.stopped_address==0);}
    {Fixture f;f.blocks[0].known[0]=2;check(f.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameter)==NBA97_PATL_METADATA);}
    {Fixture f;f.put(0x800c75c8u,Spu+1);check(f.run()==NBA97_PATL_RESOURCE&&f.progress.stopped_pc==0x8007df84u);}
    {Fixture f;check(nba97_audio_startup(nullptr,NBA97_AUDIO_STARTUP_700B0,0,f.journal.data(),256,&f.progress)==NBA97_PATL_ARGUMENT);}
}
}
int main() { startup_and_reset();registration_bugs();volume_modes();live_mutations();prefixes_and_contracts();std::printf("audio startup: %u checks passed\n",checks); }
