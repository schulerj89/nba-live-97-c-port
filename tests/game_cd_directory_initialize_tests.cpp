#include "recovered/game_cd_directory_initialize.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game CD-directory initialize check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807fff00u;
constexpr std::uint32_t EntrySp = 0x807fffd0u;
constexpr std::uint32_t FrameSp = EntrySp - 0x30u;
constexpr std::uint32_t Buffer = 0x80103550u;

struct Fixture {
    enum Mode {
        Success,
        ReadFailure,
        DelayedReady,
        NeverReady,
        UnknownReadResult,
        UnknownRootResult,
        MutateSavedReturn
    } mode = Success;
    std::size_t refuse_event = 0;
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x200000, 0xcd);
    std::vector<std::uint8_t> ram_known = std::vector<std::uint8_t>(0x200000, 1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}
    };
    Nba97GameCdDirectoryInitializeContext context{{regions,2},200,4,EntrySp,
        0x800299e0u,0x11223344u,0x800d79c8u,io,this};
    Nba97GameCdDirectoryInitializeProgress progress{};
    std::vector<Nba97GameCdDirectoryInitializeEvent> events;

    Fixture() {
        stack.fill(0xcd);
        stack_known.fill(1);
        put(0x800c4abcu,0);
    }
    std::uint8_t* byte(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.data + (address - region.base);
        check(false);
        return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.known ? region.known + (address - region.base) : nullptr;
        check(false);
        return nullptr;
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width=4) {
        for (unsigned i=0;i<width;++i) {
            *byte(address+i)=std::uint8_t(value>>(i*8));
            if (auto* mark=known(address+i)) *mark=1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width=4) {
        std::uint32_t value=0;
        for (unsigned i=0;i<width;++i)
            value|=std::uint32_t(*byte(address+i))<<(i*8);
        return value;
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameCdDirectoryInitializeEvent* event,
        Nba97GameCdDirectoryInitializeValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.events.push_back(*event);
        if (f.refuse_event && f.events.size()==f.refuse_event)
            return 0;
        if (event->kind==NBA97_CD_DIRECTORY_INITIALIZE_POLL) {
            if (f.mode!=NeverReady) f.put(Buffer+1u,0,1);
            return 1;
        }
        switch (event->entry) {
        case 0x800a4830u:
            f.put(0x800d6e2cu,event->global_pointer);
            return 1;
        case 0x800985a4u:
        case 0x8009d94cu:
            return 1;
        case 0x8009fa6cu:
            check(event->argument_count==1 && event->argument[0]==Buffer);
            f.put(Buffer+4u,0x00000200u);
            if (f.mode!=DelayedReady && f.mode!=NeverReady)
                f.put(Buffer+1u,0,1);
            value->word=f.mode==ReadFailure?0u:1u;
            value->known=f.mode==UnknownReadResult?0u:1u;
            return 1;
        case 0x80091870u:
            check(event->argument_count==1);
            if (event->argument[0]==Buffer+4u) value->word=0x100u;
            else {
                check(event->argument[0]==FrameSp+0x18u &&
                    f.get(FrameSp+0x18u)==0x00160200u);
                value->word=0x110u;
            }
            value->known=1;
            return 1;
        case 0x80091e1cu:
            check(event->argument_count==1 && event->argument[0]==0x10u);
            return 1;
        case 0x80091e80u:
            check(event->argument_count==2 && event->argument[0]==Buffer &&
                event->argument[1]==1);
            f.put(Buffer+0x9eu,23u);
            f.put(Buffer+0xa6u,2048u);
            if (f.mode==MutateSavedReturn)
                f.put(FrameSp+0x2cu,0x55667788u);
            return 1;
        case 0x800aa04cu:
            check(event->argument_count==2 && event->argument[1]==4 &&
                (event->argument[0]==Buffer+0x9eu ||
                 event->argument[0]==Buffer+0xa6u));
            value->word=f.get(event->argument[0]);
            value->known=(f.mode==UnknownRootResult &&
                event->argument[0]==Buffer+0x9eu)?0u:1u;
            return 1;
        default:
            return 0;
        }
    }
    int run() { return nba97_game_cd_directory_initialize(&context,&progress); }
};

void cold_success() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        !f.progress.cached && f.progress.return_v0==1);
    check(f.progress.operations==42 && f.progress.accesses==32 &&
        f.progress.reads==17 && f.progress.stores==15);
    check(f.progress.callbacks_completed==10 && f.progress.calls_completed==10 &&
        !f.progress.poll_callbacks_completed && !f.progress.polls);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.restored_return_address==0x800299e0u &&
        f.progress.restored_frame_pointer==0x11223344u);
    check(f.get(FrameSp+0x2cu)==0x800299e0u &&
        f.get(FrameSp+0x28u)==0x11223344u &&
        f.get(FrameSp+0x10u)==Buffer && f.get(FrameSp+0x20u)==1 &&
        f.get(FrameSp+0x24u)==Buffer);
    check(f.get(FrameSp+0x18u)==0x00160200u && f.get(Buffer+1u,1)==0);
    check(f.get(0x800d6e2cu)==0x800d79c8u &&
        f.get(0x800ebc3cu)==0x100u && f.get(0x800fb150u)==0x110u);
    check(f.get(0x800d7d3cu)==23u && f.get(0x800d7d40u)==2048u &&
        f.get(0x800c4abcu)==1);
    check(f.progress.disc_base_sector==0x100u &&
        f.progress.primary_volume_sector==0x110u &&
        f.progress.root_directory_lba==23u &&
        f.progress.root_directory_size==2048u);
    check(f.progress.disc_base_sector_known &&
        f.progress.primary_volume_sector_known &&
        f.progress.root_directory_lba_known &&
        f.progress.root_directory_size_known);
    check(f.events.size()==10 && f.events[0].pc==0x80091c2cu &&
        f.events[0].entry==0x800a4830u &&
        f.events[0].stack_pointer==FrameSp &&
        f.events[0].global_pointer==0x800d79c8u &&
        f.events[0].return_address==0x80091c34u);
    check(f.events[3].pc==0x80091c60u &&
        f.events[3].entry==0x8009fa6cu &&
        f.events[4].entry==0x80091870u &&
        f.events[5].argument[0]==FrameSp+0x18u);
    check(f.events[6].entry==0x80091e1cu && f.events[6].argument[0]==0x10u &&
        f.events[7].entry==0x80091e80u && f.events[7].argument[1]==1);
    check(f.events[8].pc==0x80091d7cu &&
        f.events[8].argument[0]==Buffer+0x9eu &&
        f.events[9].pc==0x80091da0u &&
        f.events[9].argument[0]==Buffer+0xa6u);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);
}

void cached_and_failed_read() {
    Fixture cached;
    cached.put(0x800c4abcu,1);
    cached.context.io=nullptr;
    check(cached.run()==NBA97_TEXT_COMPLETE && cached.progress.completed &&
        cached.progress.cached && cached.progress.return_v0==1);
    check(cached.progress.operations==5 && cached.progress.accesses==5 &&
        cached.progress.reads==3 && cached.progress.stores==2 &&
        !cached.progress.callbacks_completed && cached.events.empty());

    Fixture failed;
    failed.mode=Fixture::ReadFailure;
    check(failed.run()==NBA97_TEXT_COMPLETE && failed.progress.completed &&
        failed.progress.return_v0==0 && !failed.progress.cached);
    check(failed.progress.operations==15 && failed.progress.accesses==11 &&
        failed.progress.reads==6 && failed.progress.stores==5 &&
        failed.progress.calls_completed==4);
    check(failed.get(0x800c4abcu)==0 && failed.get(Buffer+1u,1)==0 &&
        failed.events.size()==4);
}

void asynchronous_poll_and_limit() {
    Fixture delayed;
    delayed.mode=Fixture::DelayedReady;
    check(delayed.run()==NBA97_TEXT_COMPLETE && delayed.progress.completed);
    check(delayed.progress.polls==1 &&
        delayed.progress.poll_callbacks_completed==1 &&
        delayed.progress.calls_completed==10 && delayed.events.size()==11);
    check(delayed.events[4].kind==NBA97_CD_DIRECTORY_INITIALIZE_POLL &&
        delayed.events[4].pc==0x80091c90u &&
        delayed.events[4].address==Buffer+1u && !delayed.events[4].entry);
    check(delayed.progress.operations==45 && delayed.progress.accesses==34 &&
        delayed.progress.reads==19 && delayed.progress.stores==15);

    Fixture limited;
    limited.mode=Fixture::NeverReady;
    limited.context.poll_budget=0;
    check(limited.run()==NBA97_TEXT_LIMIT && !limited.progress.completed &&
        limited.progress.polls==1 && !limited.progress.poll_callbacks_completed);
    check(limited.progress.stopped_pc==0x80091c90u &&
        limited.progress.stopped_address==Buffer+1u &&
        limited.progress.operations==15 && limited.progress.accesses==11 &&
        limited.progress.calls_completed==4);
}

void knownness_and_callback_prefixes() {
    Fixture unknown_read;
    unknown_read.mode=Fixture::UnknownReadResult;
    check(unknown_read.run()==NBA97_TEXT_UNKNOWN &&
        !unknown_read.progress.completed && unknown_read.events.size()==4 &&
        unknown_read.progress.callbacks_completed==4);
    check(unknown_read.progress.stopped_pc==0x80091c6cu &&
        unknown_read.progress.stopped_address==FrameSp+0x20u &&
        unknown_read.progress.accesses==9 && unknown_read.progress.reads==3 &&
        unknown_read.progress.stores==5);
    for (unsigned i=0;i<4;++i)
        check(*unknown_read.known(FrameSp+0x20u+i)==0);

    Fixture unknown_root;
    unknown_root.mode=Fixture::UnknownRootResult;
    check(unknown_root.run()==NBA97_TEXT_COMPLETE && unknown_root.progress.completed &&
        !unknown_root.progress.root_directory_lba_known &&
        unknown_root.progress.root_directory_size_known);
    for (unsigned i=0;i<4;++i)
        check(*unknown_root.known(0x800d7d3cu+i)==0);
    check(unknown_root.get(0x800c4abcu)==1);

    Fixture refused;
    refused.refuse_event=10;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && !refused.progress.completed &&
        refused.events.size()==10 && refused.progress.callbacks_completed==9 &&
        refused.progress.calls_completed==9);
    check(refused.get(0x800d7d3cu)==23u &&
        refused.get(0x800d7d40u)==0xcdcdcdcdu &&
        refused.get(0x800c4abcu)==0);

    Fixture restored;
    restored.mode=Fixture::MutateSavedReturn;
    check(restored.run()==NBA97_TEXT_COMPLETE &&
        restored.progress.restored_return_address==0x55667788u);
}

void validation_and_memory_failures() {
    {Fixture f;f.context.operation_budget=0;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x80091c0cu &&
        f.progress.stopped_address==FrameSp+0x2cu &&
        !f.progress.operations && !f.progress.accesses);}
    {Fixture f;*f.known(0x800c4abcu)=0;check(f.run()==NBA97_TEXT_UNKNOWN &&
        f.progress.stopped_pc==0x80091c1cu && f.progress.accesses==3 &&
        !f.progress.reads && f.progress.stores==2);}
    {Fixture f;f.context.stack_pointer=EntrySp+1u;check(
        f.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        f.progress.stopped_pc==0x80091c0cu);}
    {Fixture f;f.context.io=nullptr;check(f.run()==NBA97_TEXT_IO_REFUSED &&
        f.progress.stopped_pc==0x80091c2cu &&
        f.progress.stopped_entry==0x800a4830u &&
        f.progress.operations==4 && f.progress.accesses==3);}
    {Fixture f;*f.known(Buffer+1u)=2;check(f.run()==NBA97_TEXT_ARGUMENT &&
        f.progress.stopped_pc==0x80091c58u &&
        f.progress.stopped_address==Buffer+1u && f.events.size()==3);}
    {Fixture f;Nba97GameTextRegion overlap[2]={f.regions[0],f.regions[0]};
        f.context.memory={overlap,2};check(f.run()==NBA97_TEXT_ARGUMENT &&
        !f.progress.operations);}
    {Fixture f;f.regions[0].size=0;check(f.run()==NBA97_TEXT_ARGUMENT);}
    {Fixture f;f.regions[0].data=nullptr;check(f.run()==NBA97_TEXT_ARGUMENT);}
    {Fixture f;f.context.memory={nullptr,1};check(f.run()==NBA97_TEXT_ARGUMENT);}
    Nba97GameCdDirectoryInitializeProgress progress{};
    check(nba97_game_cd_directory_initialize(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_cd_directory_initialize(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    cold_success();
    cached_and_failed_read();
    asynchronous_poll_and_limit();
    knownness_and_callback_prefixes();
    validation_and_memory_failures();
    std::printf("game_cd_directory_initialize: %u checks passed\n",checks);
}
