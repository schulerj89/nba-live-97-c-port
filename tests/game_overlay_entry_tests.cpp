#include "recovered/game_overlay_entry.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value){++checks;if(!value){std::fprintf(stderr,"overlay entry check %u failed\n",checks);std::abort();}}
constexpr std::uint32_t Ram=0x80000000u,Bss=0x800d7bb8u,BssEnd=0x8010b61cu;
struct Fixture {
    enum Mode { Transfer,ReturnMain,RefuseInit,RefuseMain,UnknownSavedRa,InvalidMain } mode=Transfer;
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x200000,1);
    Nba97GameTextRegion region{Ram,bytes.data(),known.data(),bytes.size()};
    Nba97GameOverlayEntryContext context{{&region,1},100000,0x12345678u,io,this};
    Nba97GameOverlayEntryProgress progress{};
    std::vector<Nba97GameOverlayEntryEvent> calls;
    Fixture(){put(0x800c4b3cu,0x00800000u);put(0x800c4b38u,0x00008000u);
        for(std::uint32_t a=Bss;a<BssEnd;++a)bytes[a-Ram]=0xa5;}
    void put(std::uint32_t address,std::uint32_t value,unsigned width=4){
        for(unsigned i=0;i<width;++i){bytes[address-Ram+i]=std::uint8_t(value>>(i*8));known[address-Ram+i]=1;}}
    std::uint32_t get(std::uint32_t address)const{std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes[address-Ram+i])<<(i*8);
        return value;}
    static int io(void* user,const Nba97GameTextMemory*,const Nba97GameOverlayEntryEvent* event,
        Nba97GameOverlayEntryCalleeOutcome* outcome){
        auto& f=*static_cast<Fixture*>(user);f.calls.push_back(*event);
        if(event->kind==NBA97_GAME_OVERLAY_BIOS_A0_39_INIT_HEAP){
            if(f.mode==RefuseInit)return 0;
            *outcome=NBA97_GAME_OVERLAY_CALLEE_RETURNED;
            if(f.mode==UnknownSavedRa)for(unsigned i=0;i<4;++i)f.known[Bss-Ram+i]=0;
            else f.put(Bss,0xcafebabeu);
            return 1;
        }
        if(f.mode==RefuseMain)return 0;
        if(f.mode==InvalidMain){*outcome=static_cast<Nba97GameOverlayEntryCalleeOutcome>(9);return 1;}
        *outcome=f.mode==ReturnMain?NBA97_GAME_OVERLAY_CALLEE_RETURNED:
            NBA97_GAME_OVERLAY_CALLEE_TRANSFERRED;return 1;
    }
    int run(){return nba97_game_overlay_entry(&context,&progress);}
};
void ordinary(){
    Fixture f;check(f.run()==NBA97_TEXT_COMPLETE&&f.progress.completed&&f.progress.transferred);
    check(f.progress.words_cleared==(BssEnd-Bss)/4&&f.progress.stores==52892&&f.progress.accesses==52895);
    check(f.get(Bss)==0xcafebabeu);for(std::uint32_t a=Bss+4;a<BssEnd;a+=4)check(f.get(a)==0);
    check(f.get(0x800c4b18u)==0x8010b61cu&&f.get(0x800c4b1cu)==0x006ec9dcu);
    check(f.progress.stack_pointer==0x807ffff8u&&f.progress.frame_pointer==0x807ffff8u&&
        f.progress.global_pointer==0x800d79c8u);
    check(f.progress.heap_base==0x8010b61cu&&f.progress.heap_size==0x006ec9dcu);
    check(f.progress.saved_return_address==0x12345678u&&f.progress.restored_return_address==0xcafebabeu);
    check(f.calls.size()==2&&f.progress.callbacks_completed==2&&f.progress.entered_main);
    check(f.calls[0].pc==0x800948b0u&&f.calls[0].entry==0x80098554u&&
        f.calls[0].argument_count==2&&f.calls[0].argument[0]==0x8010b620u&&
        f.calls[0].argument[1]==0x006ec9dcu&&f.calls[0].return_address==0x800948b8u);
    check(f.calls[1].pc==0x800948c4u&&f.calls[1].entry==0x80029994u&&
        f.calls[1].argument_count==0&&f.calls[1].return_address==0x800948ccu);
    check(!f.progress.stopped_pc&&!f.progress.stopped_address&&!f.progress.stopped_entry&&!f.progress.trapped);
}
void traps_and_refusals(){
    {Fixture f;f.mode=Fixture::ReturnMain;check(f.run()==NBA97_GAME_OVERLAY_ENTRY_BREAK_TRAP);
        check(f.progress.entered_main&&f.progress.trapped&&!f.progress.completed&&f.progress.stopped_pc==0x800948ccu);}
    {Fixture f;f.mode=Fixture::RefuseInit;check(f.run()==NBA97_TEXT_IO_REFUSED&&f.progress.stopped_pc==0x800948b0u&&
        f.progress.stopped_entry==0x80098554u&&f.progress.callbacks_completed==0);check(f.get(Bss)==0x12345678u);}
    {Fixture f;f.mode=Fixture::RefuseMain;check(f.run()==NBA97_TEXT_IO_REFUSED&&f.progress.stopped_pc==0x800948c4u&&
        f.progress.stopped_entry==0x80029994u&&f.progress.callbacks_completed==1&&!f.progress.entered_main);}
    {Fixture f;f.mode=Fixture::UnknownSavedRa;check(f.run()==NBA97_TEXT_UNKNOWN&&f.progress.stopped_pc==0x800948bcu&&
        f.progress.stopped_address==Bss&&f.progress.callbacks_completed==1);}
    {Fixture f;f.mode=Fixture::InvalidMain;check(f.run()==NBA97_TEXT_ARGUMENT&&f.progress.stopped_pc==0x800948c4u&&
        f.progress.callbacks_completed==1);}
    {Fixture f;f.put(0x800c4b3cu,0x80000003u);check(f.run()==NBA97_GAME_OVERLAY_ENTRY_ARITHMETIC_TRAP&&
        f.progress.trapped&&f.progress.stopped_pc==0x80094858u&&f.progress.stores==(BssEnd-Bss)/4);}
}
void memory_prefixes(){
    {Fixture f;f.context.access_budget=0;check(f.run()==NBA97_TEXT_LIMIT&&f.progress.stopped_pc==0x80094838u&&
        f.progress.stopped_address==Bss&&!f.progress.stores);}
    {Fixture f;f.context.access_budget=(BssEnd-Bss)/4;check(f.run()==NBA97_TEXT_LIMIT&&
        f.progress.words_cleared==(BssEnd-Bss)/4&&f.progress.stopped_pc==0x80094850u);}
    {Fixture f;f.context.access_budget=(BssEnd-Bss)/4+3;check(f.run()==NBA97_TEXT_LIMIT&&
        f.progress.stopped_pc==0x80094898u&&f.progress.stores==(BssEnd-Bss)/4+1);}
    {Fixture f;f.context.io=nullptr;check(f.run()==NBA97_TEXT_IO_REFUSED&&f.progress.stopped_pc==0x800948b0u);}
    {Fixture f;f.region.size=0x1000;check(f.run()==NBA97_TEXT_RESOURCE&&f.progress.stopped_address==Bss);}
    {Fixture f;f.known[Bss-Ram]=2;check(f.run()==NBA97_TEXT_ARGUMENT&&!f.progress.stores);}
    {Fixture f;Nba97GameTextRegion duplicate=f.region;Nba97GameTextRegion regions[]={f.region,duplicate};
        f.context.memory={regions,2};check(f.run()==NBA97_TEXT_ARGUMENT&&!f.progress.accesses);}
    Nba97GameOverlayEntryProgress progress{};check(nba97_game_overlay_entry(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;check(nba97_game_overlay_entry(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}
int main(){ordinary();traps_and_refusals();memory_prefixes();
    std::printf("game_overlay_entry: %u checks passed\n",checks);}
