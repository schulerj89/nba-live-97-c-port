#include "recovered/spu_transfer.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;if(!value) { std::fprintf(stderr,"spu transfer check %u failed\n",checks);std::exit(1); }
}
constexpr uint32_t Globals=0x800c7500u,Source=0x80110000u,Device=0x1f801c00u;
constexpr uint32_t Address=Device+0x1a6u,Control=Device+0x1aau,Status=Device+0x1aeu;
constexpr uint32_t Madr=0x1f8010c0u,Bcr=Madr+4u,Chcr=Madr+8u,SpuBusDelay=0x1f801014u;
struct Fixture {
    std::array<uint8_t,0x200> bytes{},mask{};
    std::array<uint8_t,128> source{},sourceMask{};
    std::array<Nba97VoicePatlSpan,2> spans{};
    std::map<uint32_t,uint32_t> registers;
    std::vector<uint32_t> fifo;
    std::vector<Nba97SpuTransferEvent> journal;
    Nba97SpuTransfer owner{};
    Nba97SpuTransferProgress progress{};
    std::function<int(const Nba97SpuTransferEvent&,Nba97SpuTransferValue&)> hook;
    unsigned diagnostics=0,deliveries=0,callbacks=0;
    bool pending=false;
    Fixture():journal(40000) {
        bytes.fill(0xcd);sourceMask.fill(1);
        for(size_t i=0;i<source.size();++i)source[i]=static_cast<uint8_t>(i^0x5a);
        spans[0]={bytes.data(),mask.data(),bytes.size(),Globals,1,1,0};
        spans[1]={source.data(),sourceMask.data(),source.size(),Source,1,1,0};
        owner={{spans.data(),spans.size()},invoke,this,200000};
        // Explicit CPU/device fixture entry, not original startup or a sample
        // backend. Scripted device results test the recovered CPU boundaries.
        put(0x800c75c8u,Device);put(0x800c75ecu,3);put(0x800c75e0u,0);
        put(0x800c75ccu,Madr);put(0x800c75d0u,Bcr);put(0x800c75d4u,Chcr);
        put(0x800c75dcu,SpuBusDelay);put(0x800c7614u,0);put(0x800c75fcu,0);
        put(0x800c75c4u,0x202,2);
        registers[Address]=0x202;registers[Control]=0;registers[Status]=0;
        registers[SpuBusDelay]=0xf1234567;
    }
    void put(uint32_t at,uint32_t value,uint32_t width=4) {
        check(nba97_voice_patl_write(&owner.memory,at,width,value)==1);
    }
    uint32_t get(uint32_t at,uint32_t width=4) {
        uint32_t value=0;check(nba97_voice_patl_read(&owner.memory,at,width,&value)==1);return value;
    }
    static int invoke(void* context,const Nba97VoicePatlMemory* memory,
        const Nba97SpuTransferEvent* e,Nba97SpuTransferValue* result) {
        auto& f=*static_cast<Fixture*>(context);check(memory==&f.owner.memory);
        if(f.hook) { int rc=f.hook(*e,*result);if(rc!=-99)return rc; }
        switch(e->kind) {
        case NBA97_SPU_TRANSFER_DEVICE_READ: {
            auto it=f.registers.find(e->address);if(it==f.registers.end())return 0;
            *result={it->second,1};return 1;
        }
        case NBA97_SPU_TRANSFER_DEVICE_WRITE:
            if(e->address==Device+0x1a8u)f.fifo.push_back(e->value);
            else f.registers[e->address]=e->value;
            return 1;
        case NBA97_SPU_TRANSFER_DIAGNOSTIC_83B20:
            ++f.diagnostics;*result={0x87654321,1};return 1;
        case NBA97_SPU_TRANSFER_DELIVER_EVENT_B0_07:
            check(e->argument[0]==0xf0000009u&&e->argument[1]==0x20);
            ++f.deliveries;f.pending=true;*result={7,1};return 1;
        case NBA97_SPU_TRANSFER_CALLBACK_7D668:
            ++f.callbacks;*result={0xabcdef01,1};return 1;
        case NBA97_SPU_TRANSFER_TEST_EVENT_B0_0B:
            if(e->address!=0x12345678u)return 0;
            *result={f.pending?1u:0u,1};f.pending=false;return 1;
        default:return 0;
        }
    }
    int run(Nba97SpuTransferOperation op,uint32_t a0=0,uint32_t a1=0,uint32_t a2=0,size_t capacity=40000) {
        return nba97_spu_transfer(&owner,op,a0,a1,a2,journal.data(),capacity,&progress);
    }
};
void dma_commands() {
    for(uint32_t count:{0u,1u,63u,64u,65u,0x400000u,0x400001u,0xffffffffu}) {
        Fixture f;check(f.run(NBA97_SPU_TRANSFER_7DC90,Source,count)==1);
        check(f.progress.completed&&f.progress.returned.known&&f.progress.returned.word==count);
        check(f.get(0x800c7614u)==0&&f.get(0x800c7618u)==Source);
        check(f.get(0x800c761cu)==(count>>6)+((count&63u)?1u:0u));
        const uint32_t encoded=((count>>6)+((count&63u)?1u:0u))<<16;
        check(f.registers[Bcr]==(encoded|16u)&&f.registers[Madr]==Source);
        check(f.registers[Chcr]==0x01000201&&f.registers[SpuBusDelay]==0xf0234567);
        check(f.registers[Address]==0x202&&f.deliveries==0&&!f.pending);
        // Zero and overflowing block counts are literal register requests,
        // not permission for a backend to claim a successful zero-byte copy.
    }
    {Fixture f;f.registers[Control]=0xa004;
        check(f.run(NBA97_SPU_CONTROL_7D9E8,0)==1&&f.registers[Control]==0xa034);
        check(f.get(0x800c7614u)==1);
        check(f.run(NBA97_SPU_CONTROL_7D9E8,3,Source,65)==1);
        check(f.registers[Chcr]==0x01000200&&f.registers[SpuBusDelay]==0xf2234567);}
    {Fixture f;f.put(0x800c75ecu,35);
        check(f.run(NBA97_SPU_CONTROL_7D9E8,2,0x12345678)==1);
        check(f.get(0x800c75c4u,2)==0x8acf&&f.registers[Address]==0x8acf);}
    for(uint32_t command:{4u,0xffffffffu,0x80000000u}) {
        Fixture f;f.owner.memory={nullptr,0};f.owner.io=nullptr;
        check(f.run(NBA97_SPU_CONTROL_7D9E8,command,0,0,0)==1);
        check(f.progress.returned.word==0&&f.progress.accesses==0&&f.progress.events==0);
    }
}
void timeout_returns() {
    {Fixture f;unsigned polls=0;f.registers[Address]=0;
        f.hook=[&](const auto& e,auto&) { if(e.kind==NBA97_SPU_TRANSFER_DEVICE_READ&&e.address==Address)++polls;return -99; };
        check(f.run(NBA97_SPU_CONTROL_7D9E8,1)==1);
        check(f.progress.returned.word==0xfffffffeu&&polls==0xf01);
        check(f.get(0x800c7614u)==0&&f.registers[Control]==0);}
    {Fixture f;unsigned starts=0;
        f.hook=[&](const auto& e,auto& result) {
            if(e.kind==NBA97_SPU_TRANSFER_DEVICE_READ&&e.address==Address) { result={0,1};return 1; }
            if(e.kind==NBA97_SPU_TRANSFER_DEVICE_WRITE&&e.address==Chcr)++starts;
            return -99;
        };
        // The original wrapper ignores both lower timeout results, reports
        // its requested count and never reaches the CHCR start in this case.
        check(f.run(NBA97_SPU_TRANSFER_7DC90,Source,65)==1);
        check(f.progress.returned.word==65&&starts==0&&f.registers.find(Bcr)==f.registers.end());}
}
void pio_quirks() {
    {Fixture f;check(f.run(NBA97_SPU_PIO_7D334,Source,1)==1);
        check(f.fifo.size()==1&&f.fifo[0]==0x5b5a&&f.progress.returned.word==0);}
    {Fixture f;f.spans[1].size=1;
        check(f.run(NBA97_SPU_PIO_7D334,Source,1)==NBA97_PATL_RESOURCE);
        check(f.progress.stopped_pc==0x8007d3f4u&&f.fifo.empty()&&f.registers[Address]==0x202);}
    {Fixture f;f.sourceMask[0]=0;f.sourceMask[1]=2;
        check(f.run(NBA97_SPU_PIO_7D334,Source,1)==NBA97_PATL_METADATA&&f.fifo.empty());}
    {Fixture f;f.sourceMask[1]=0;
        check(f.run(NBA97_SPU_PIO_7D334,Source,1)==NBA97_PATL_RESOURCE&&f.fifo.empty());}
    {Fixture f;unsigned statusReads=0;
        f.hook=[&](const auto& e,auto& result) {
            if(e.kind==NBA97_SPU_TRANSFER_DEVICE_READ&&e.address==Status) {
                result={statusReads++==0?0u:0x400u,1};return 1;
            }
            return -99;
        };
        // Busy timeout prints and proceeds to the next chunk. Final status
        // timeout returns the actual diagnostic result, not an invented code.
        check(f.run(NBA97_SPU_PIO_7D334,Source,65)==1);
        check(f.fifo.size()==33&&f.diagnostics==3&&f.progress.returned.word==0x87654321);
        check(f.get(0x800c75c0u)==5001);
    }
    {Fixture f;unsigned reads=0;
        f.hook=[&](const auto& e,auto& result) {
            if(e.kind==NBA97_SPU_TRANSFER_DEVICE_READ&&e.address==Status) { result={reads++?1u:0u,1};return 1; }
            if(e.kind==NBA97_SPU_TRANSFER_DIAGNOSTIC_83B20) { result={0,0};return 1; }
            return -99;
        };
        check(f.run(NBA97_SPU_PIO_7D334,Source,0)==1&&!f.progress.returned.known);
        reads=0;f.put(0x800c75e0u,1);
        check(f.run(NBA97_SPU_TRANSFER_7DC90,Source,0)==1&&f.progress.returned.known&&f.progress.returned.word==0);
    }
}
void completion_paths() {
    {Fixture f;f.registers[Control]=0x8030;
        check(f.run(NBA97_SPU_ISR_7D668)==1&&f.registers[Control]==0x8000);
        check(f.deliveries==1&&f.callbacks==0&&f.progress.returned.word==7);
        check(f.run(NBA97_SPU_TEST_EVENT_7F568,0x12345678)==1&&f.progress.returned.word==1);
        check(f.run(NBA97_SPU_TEST_EVENT_7F568,0x12345678)==1&&f.progress.returned.word==0);}
    {Fixture f;f.put(0x800c75fcu,0x80012340);f.put(0x800c7614u,1);
        check(f.run(NBA97_SPU_ISR_7D668)==1&&f.callbacks==1&&f.deliveries==0);
        check(f.progress.returned.word==0xabcdef01);
        const auto& last=f.journal[f.progress.events-1];
        check(last.kind==NBA97_SPU_TRANSFER_CALLBACK_7D668&&last.address==0x80012340&&last.argument[0]==0xf0000000u);}
    {Fixture f;unsigned reads=0;
        f.hook=[&](const auto& e,auto& result) {
            if(e.kind==NBA97_SPU_TRANSFER_DEVICE_READ&&e.address==Control) { ++reads;result={0x30,1};return 1; }
            return -99;
        };
        check(f.run(NBA97_SPU_ISR_7D668)==1&&f.deliveries==1&&reads==0xf02);}
    {Fixture f;f.hook=[](const auto& e,auto&) { return e.kind==NBA97_SPU_TRANSFER_DELIVER_EVENT_B0_07?0:-99; };
        f.registers[Control]=0x30;
        check(f.run(NBA97_SPU_ISR_7D668)==NBA97_PATL_IO_REFUSED);
        check(f.registers[Control]==0&&f.deliveries==0&&!f.progress.completed);
        check(f.journal[f.progress.events-1].pc==0x8007f50cu&&!f.journal[f.progress.events-1].completed);}
    {Fixture f;check(f.run(NBA97_SPU_DELIVER_EVENT_7F508,0xf0000009u,0x20)==1&&f.pending);}
}
void native_refusals_and_prefixes() {
    {Fixture f;f.owner.io=nullptr;
        check(f.run(NBA97_SPU_TRANSFER_7DC90,Source,64)==NBA97_PATL_IO_REFUSED);
        check(f.progress.stores==1&&f.progress.callbacks_completed==0&&!f.progress.completed);}
    {Fixture f;f.hook=[](const auto& e,auto& result) {
        if(e.kind==NBA97_SPU_TRANSFER_DEVICE_READ) { result={0,0};return 1; }return -99;
        };
        check(f.run(NBA97_SPU_CONTROL_7D9E8,1)==NBA97_PATL_RESOURCE);
        check(f.progress.callbacks_completed==1&&f.progress.stores==0);}
    {Fixture f;f.hook=[](const auto&,auto& result) { result={0,2};return 1; };
        check(f.run(NBA97_SPU_TEST_EVENT_7F568,0x12345678)==NBA97_PATL_METADATA);
        check(f.progress.callbacks_completed==1&&!f.progress.completed);}
    {Fixture f;f.put(0x800c75c8u,Device+1);
        check(f.run(NBA97_SPU_CONTROL_7D9E8,2,0x1010)==NBA97_PATL_RESOURCE);
        check(f.progress.stores==1&&f.progress.callbacks_completed==0&&f.get(0x800c75c4u,2)==0x202);}
    Fixture baseline;check(baseline.run(NBA97_SPU_TRANSFER_7DC90,Source,65)==1);
    const size_t events=baseline.progress.events,accesses=baseline.progress.accesses;
    for(size_t capacity=0;capacity<events;++capacity) {
        Fixture f;check(f.run(NBA97_SPU_TRANSFER_7DC90,Source,65,0,capacity)==NBA97_SPU_TRANSFER_LIMIT);
        check(f.progress.events==capacity&&!f.progress.completed);
        for(size_t i=0;i<capacity;++i) {
            const auto& a=f.journal[i];const auto& b=baseline.journal[i];
            check(a.completed&&a.pc==b.pc&&a.kind==b.kind&&a.address==b.address&&a.value==b.value);
        }
    }
    for(size_t budget=0;budget<accesses;++budget) {
        Fixture f;f.owner.access_budget=budget;
        check(f.run(NBA97_SPU_TRANSFER_7DC90,Source,65)==NBA97_SPU_TRANSFER_LIMIT);
        check(f.progress.accesses==budget&&!f.progress.completed&&f.progress.events<=events);
    }
    {Fixture f;check(nba97_spu_transfer(nullptr,NBA97_SPU_ISR_7D668,0,0,0,nullptr,0,&f.progress)==NBA97_PATL_ARGUMENT);
        check(nba97_spu_transfer(&f.owner,NBA97_SPU_ISR_7D668,0,0,0,nullptr,1,&f.progress)==NBA97_PATL_ARGUMENT);}
}
}
int main() {
    dma_commands();timeout_returns();pio_quirks();completion_paths();native_refusals_and_prefixes();
    std::printf("spu transfer: %u checks passed\n",checks);
}
