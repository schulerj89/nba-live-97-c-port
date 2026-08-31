#include "recovered/interrupt_controller.h"
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
void check_at(bool value,int line) { ++checks;if(!value) { std::fprintf(stderr,"interrupt controller check %u failed at line %d\n",checks,line);std::exit(1); } }
#define check(v) check_at((v),__LINE__)
constexpr uint32_t Base=0x800c7d80u,Table=0x800c7dacu,Ready=0x800c7dc8u,Callbacks=0x800c7dccu;
constexpr uint32_t Cache=0x800c7df8u,Context=0x800c7dfcu,StatusPointer=0x800c7e2cu,MaskPointer=0x800c7e30u;
constexpr uint32_t Counter=0x800c7e34u,DmaReady=0x800c7e38u,DmaCallbacks=0x800c7e3cu,DmaPointer=0x800c7e5cu;
constexpr uint32_t VReady=0x800c7e68u,VCallback=0x800c7e6cu,VCount=0x800c7e70u,Data=0x80110000u;
constexpr uint32_t Status=0x1f801070u,Mask=0x1f801074u,Dma=0x1f8010f4u,Timer=0x1f801114u;
struct Fixture {
    std::array<uint8_t,0x200> bytes{},known{};
    std::array<uint8_t,64> data{},dataKnown{};
    std::array<Nba97VoicePatlSpan,2> spans{};
    std::array<Nba97InterruptEvent,1024> journal{};
    Nba97InterruptController owner{};Nba97InterruptProgress progress{};
    std::map<uint32_t,uint32_t> device;
    std::vector<uint32_t> callbacks;
    std::function<int(const Nba97InterruptEvent&,Nba97SpuTransferValue&)> hook;
    Nba97SpuTransferValue captured{0,1};int exceptionResult=2;
    Fixture() {
        bytes.fill(0xcd);data.fill(0xa5);
        spans[0]={bytes.data(),known.data(),bytes.size(),Base,1,1,0};
        spans[1]={data.data(),dataKnown.data(),data.size(),Data,1,1,0};
        owner={{spans.data(),spans.size()},io,this,10000};
        // Explicit static FE pointers and incoming hardware/BIOS test state.
        // The actual recovered controller publishes all ready/callback fields.
        put(0x800c7dc4u,Table);put(Table+4,0);put(Table+8,0x8007f9bcu);put(Table+0x14,0);
        put(Ready,0,2);put(StatusPointer,Status);put(MaskPointer,Mask);put(DmaPointer,Dma);
        put(0x800c7e64u,Timer);put(VCount,0xfffffffdu);
        device[Status]=0;device[Mask]=0;device[Dma]=0;device[Timer]=0;
    }
    void put(uint32_t at,uint32_t value,uint32_t width=4) { check(nba97_voice_patl_write(&owner.memory,at,width,value)==1); }
    uint32_t get(uint32_t at,uint32_t width=4) { uint32_t v=0;check(nba97_voice_patl_read(&owner.memory,at,width,&v)==1);return v; }
    void initialize() { check(run(NBA97_INTERRUPT_INITIALIZE_7F708)==1); }
    int run(Nba97InterruptOperation op,uint32_t a0=0,uint32_t a1=0,Nba97SpuTransferValue v1={0,0},size_t capacity=1024) {
        callbacks.clear();return nba97_interrupt_controller(&owner,op,a0,a1,v1,journal.data(),capacity,&progress);
    }
    static int io(void* p,const Nba97VoicePatlMemory* memory,const Nba97InterruptEvent* e,Nba97SpuTransferValue* out) {
        auto& f=*static_cast<Fixture*>(p);check(memory==&f.owner.memory);
        if(f.hook) { auto rc=f.hook(*e,*out);if(rc!=-99)return rc; }
        if(e->kind==NBA97_INTERRUPT_DEVICE_READ) {
            if(e->address>=Base&&e->address<0x80200000u) {
                auto rc=nba97_voice_patl_read(memory,e->address,e->width,&out->word);out->known=1;return rc==1;
            }
            auto it=f.device.find(e->address);if(it==f.device.end())return 0;
            *out={it->second,1};return 1;
        }
        if(e->kind==NBA97_INTERRUPT_DEVICE_WRITE) {
            if(e->address>=Base&&e->address<0x80200000u)return nba97_voice_patl_write(memory,e->address,e->width,e->value)==1;
            // Scripted acknowledgment fixture for CPU ordering, not evidence
            // of device/BIOS equivalence or a production IRQ scheduler.
            if(e->address==Status)f.device[Status]&=e->value;
            else if(e->address==Dma)f.device[Dma]=(f.device[Dma]&0x7f000000u&~e->value)|(e->value&0x00ffffffu);
            else f.device[e->address]=e->value;
            return 1;
        }
        if(e->kind==NBA97_INTERRUPT_CAPTURE_CONTEXT) { check(e->argument[0]==Context);*out=f.captured;return 1; }
        if(e->kind==NBA97_INTERRUPT_RETURN_EXCEPTION) { *out={0x55,0};return f.exceptionResult; }
        if(e->kind==NBA97_INTERRUPT_CALLBACK)f.callbacks.push_back(e->address);
        if(e->kind==NBA97_INTERRUPT_EXIT_CRITICAL) { *out={0xfeedface,0};return 1; }
        *out={0x12345678,1};return 1; // Explicit scripted lower API result.
    }
};
void startup_shutdown() {
    Fixture f;f.initialize();
    check(f.progress.completed&&!f.progress.transferred&&!f.progress.returned.known);
    check(f.get(Ready,2)==1&&f.get(Ready+2,2)==0&&f.get(Cache)==9);
    check(f.get(Table+4)==0x8007fdb8u&&f.get(Table+0x14)==0x8007ffe8u);
    check(f.get(Callbacks)==0x8007ff9cu&&f.get(Callbacks+12)==0x8007fc54u);
    check(f.get(DmaReady,2)==1&&f.get(VReady,2)==1&&f.get(VCallback)==0);
    check(f.get(VCount)==0xfffffffdu&&f.get(Context+4)==0x800dad08u);
    check(f.device[Mask]==9&&f.device[Timer]==0x107);
    check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==1&&f.progress.accesses==1&&f.progress.events==0);
    check(f.progress.returned.known&&f.progress.returned.word==1);
    auto pointer=f.get(Table+4);
    check(f.run(NBA97_INTERRUPT_SHUTDOWN_7FAE4)==1&&f.get(Ready,2)==0&&f.get(DmaReady,2)==0&&f.get(VReady,2)==0);
    check(f.get(Table+4)==pointer&&f.get(Table+0x14)==0x8007ffe8u&&f.get(Context+4)==0);
    check(f.get(VCount)==0xfffffffdu&&f.device[Mask]==0&&f.device[Dma]==0);
    f.initialize();check(f.get(Cache)==9&&f.get(Context+4)==0x800dad08u);
}
void guards_and_failures() {
    for(uint32_t ready:{1u,2u,0xffffu}) { Fixture f;f.put(Ready,ready,2);check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==1);check(f.progress.returned.word==ready&&f.progress.events==0); }
    {Fixture f;f.captured={0x12345678,0};check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_RESOURCE);
        check(f.progress.stopped_pc==0x80083b34u&&f.progress.callbacks_completed==2&&f.progress.stores==25);
        check(f.get(Ready,2)==0&&f.get(Table+4)==0);}
    {Fixture f;f.captured={0,2};check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_METADATA);
        check(f.journal[f.progress.events-1].completed&&f.progress.stores==25);}
    {Fixture f;f.hook=[](const Nba97InterruptEvent& e,Nba97SpuTransferValue&){return e.kind==NBA97_INTERRUPT_REMOVE_CDROM_DRIVER?0:-99;};
        check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_IO_REFUSED);
        check(f.get(Ready,2)==1&&f.get(Table+4)==0x8007fdb8u);
        check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==1&&f.progress.events==0);}
    {Fixture f;f.owner.io=nullptr;check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_IO_REFUSED&&f.progress.stores==0);}
    {Fixture f;f.hook=[](const Nba97InterruptEvent& e,Nba97SpuTransferValue& v){if(e.kind==NBA97_INTERRUPT_ENTER_CRITICAL){v={0,1};return 2;}return -99;};
        check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_METADATA&&f.progress.callbacks_completed==1);
        check(f.journal[0].completed&&f.journal[0].transferred&&!f.progress.completed);}
}
void capture_transfer() {
    Fixture f;f.captured={1,1};check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==2);
    check(f.progress.completed&&f.progress.transferred&&!f.progress.returned.known);
    check(f.progress.stopped_pc==0x8007fb9cu&&f.get(Ready,2)==0&&f.get(Table+4)==0);
    check(f.get(Ready+2,2)==0&&f.journal[f.progress.events-1].transferred);
    f.exceptionResult=1;check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==1);
    check(f.get(Ready,2)==1&&f.get(Table+4)==0x8007fdb8u); // Explicit returning-ROM fixture only.
}
void registration_aliases() {
    Fixture f;f.initialize();
    check(f.run(NBA97_INTERRUPT_REGISTER_7F9BC,4,0x80012340u)==1&&f.progress.returned.word==0);
    check(f.get(Callbacks+16)==0x80012340u&&f.get(Cache)==25&&f.device[Mask]==25);
    check(f.journal[f.progress.events-1].kind==NBA97_INTERRUPT_CHANGE_CLEAR_COUNTER);
    check(f.journal[f.progress.events-1].argument[0]==0&&f.journal[f.progress.events-1].argument[1]==0);
    check(f.run(NBA97_INTERRUPT_REGISTER_7F9BC,4,0)==1&&f.progress.returned.word==0x80012340u);
    check(f.device[Mask]==9&&f.journal[f.progress.events-1].argument[1]==1);
    check(f.run(NBA97_INTERRUPT_REGISTER_7F9BC,11,0x200u)==1); // Slot aliases cached mask.
    check(f.progress.returned.word==9&&f.get(Cache)==0xa00&&f.device[Mask]==0x809);
    f.put(MaskPointer,Mask);f.put(Data,0x11,2);
    check(f.run(NBA97_INTERRUPT_REGISTER_7F9BC,25,Data)==1); // Slot aliases live I_MASK pointer.
    check(f.get(MaskPointer)==Data&&f.get(Data,2)==0x809&&f.device[Mask]==0);
    {Fixture g;g.put(Callbacks,0x22);g.put(Ready,0,2);check(g.run(NBA97_INTERRUPT_REGISTER_7F9BC,0,0x33)==1&&g.progress.returned.word==0x22&&g.progress.events==0);}
    {Fixture g;g.put(Table+8,0x80012340);check(g.run(NBA97_INTERRUPT_DISPATCH_7F600,7,0x456)==1);
        auto& e=g.journal[g.progress.events-1];check(e.kind==NBA97_INTERRUPT_OTHER_DISPATCH&&e.address==0x80012340&&e.argument[0]==7&&e.argument[1]==0x456);}
}
void interrupt_dispatch() {
    Fixture f;f.initialize();f.put(VCallback,0x80011110);f.put(DmaCallbacks+16,0x80022220);
    f.device[Status]=9;f.device[Dma]=0x10100000;
    check(f.run(NBA97_INTERRUPT_HANDLE_7F7C8)==2&&f.progress.transferred);
    check(f.callbacks==std::vector<uint32_t>({0x80011110,0x80022220}));
    check(f.get(VCount)==0xfffffffeu&&f.device[Status]==0&&f.device[Dma]==0x100000);
    check(f.get(Ready+2,2)==0&&f.get(Counter)==0);
    {Fixture g;g.initialize();g.put(Callbacks+4,0x80033330);g.put(Cache,2);g.device[Mask]=2;g.device[Status]=2;
        g.hook=[&](const Nba97InterruptEvent& e,Nba97SpuTransferValue& v){if(e.kind==NBA97_INTERRUPT_CALLBACK){check(g.device[Status]==0);v={0,0};return 2;}return -99;};
        check(g.run(NBA97_INTERRUPT_HANDLE_7F7C8)==2&&g.get(Ready+2,2)==1);}
    {Fixture g;g.initialize();g.put(Cache,0x8000);g.device[Status]=0x8000;g.device[Mask]=0x8000;g.owner.access_budget=100;
        check(g.run(NBA97_INTERRUPT_HANDLE_7F7C8)==NBA97_INTERRUPT_LIMIT&&g.get(Ready+2,2)==1);}
}
void diagnostic_thresholds() {
    for(uint32_t count:{0u,0x100u,0x101u,0xffffffffu,0x80000000u}) {
        Fixture f;f.initialize();f.put(Cache,0);f.put(Counter,count);f.device[Status]=2;f.device[Mask]=2;
        check(f.run(NBA97_INTERRUPT_HANDLE_7F7C8)==2);
        bool diagnosed=!(count&0x80000000u)&&count>=0x101u;
        check(f.get(Counter)==(diagnosed?0u:count+1u));check(f.device[Mask]==(diagnosed?0u:2u));
        unsigned diagnostics=0;for(size_t i=0;i<f.progress.events;++i)if(f.journal[i].kind==NBA97_INTERRUPT_DIAGNOSTIC)++diagnostics;
        check(diagnostics==(diagnosed?1u:0u));
    }
    for(uint32_t value:{0u,0x8000u,0x80000000u}) {
        Fixture f;f.device[Dma]=value;check(f.run(NBA97_INTERRUPT_DMA_HANDLE_7FC54)==1);
        if(!value)check(f.progress.returned.known&&f.progress.returned.word==0);
        else { auto& e=f.journal[f.progress.events-1];check(e.kind==NBA97_INTERRUPT_DIAGNOSTIC&&e.argument[0]==0x80027ee8u&&e.argument[1]==value); }
    }
}
void vblank_and_limits() {
    Fixture f;f.put(VReady,0,2);
    check(f.run(NBA97_INTERRUPT_VBLANK_SET_7FFE8,0x1234,0,{0xdeadbeef,0})==1&&!f.progress.returned.known&&f.progress.returned.word==0xdeadbeef);
    check(f.run(NBA97_INTERRUPT_VBLANK_SET_7FFE8,0,0,{4,2})==NBA97_PATL_METADATA&&f.progress.accesses==1);
    f.put(VReady,1,2);f.put(VCallback,0x2222);
    check(f.run(NBA97_INTERRUPT_VBLANK_SET_7FFE8,0x1234,0,{4,2})==1&&f.progress.returned.word==0x2222);
    f.put(VCallback,0);f.put(VCount,0xffffffffu);check(f.run(NBA97_INTERRUPT_VBLANK_HANDLE_7FF9C)==1&&f.progress.returned.word==0);
    f.put(VCallback,0x8007ff9cu);f.owner.access_budget=100000;
    check(f.run(NBA97_INTERRUPT_VBLANK_HANDLE_7FF9C)==NBA97_INTERRUPT_LIMIT&&f.get(VCount)==65);
    check(f.progress.stopped_pc==0x8007ffd0u&&f.progress.stopped_address==0x8007ff9cu);
    for(auto op:{NBA97_INTERRUPT_CLEAR_7FB4C,NBA97_INTERRUPT_CLEAR_7FEC0,NBA97_INTERRUPT_CLEAR_80020}) {
        Fixture g;check(g.run(op,Data,0)==1&&g.progress.returned.word==0xffffffffu&&g.progress.accesses==0);
        check(g.run(op,Data,16)==1&&g.progress.stores==16&&g.get(Data+60)==0);
        g.owner.access_budget=3;check(g.run(op,Data,0xffffffffu)==NBA97_INTERRUPT_LIMIT&&g.progress.stores==3);
    }
    for(size_t budget:{0u,1u,2u,10u,25u,40u}) { Fixture g;g.owner.access_budget=budget;
        check(g.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_INTERRUPT_LIMIT&&g.progress.accesses==budget); }
    for(size_t cap:{0u,1u,2u,10u,25u}) { Fixture g;
        check(g.run(NBA97_INTERRUPT_INITIALIZE_7F708,0,0,{0,0},cap)==NBA97_INTERRUPT_LIMIT&&g.progress.events==cap); }
}
}
int main() {
    startup_shutdown();guards_and_failures();capture_transfer();registration_aliases();interrupt_dispatch();diagnostic_thresholds();vblank_and_limits();
    std::printf("interrupt controller: %u checks passed\n",checks);return 0;
}
