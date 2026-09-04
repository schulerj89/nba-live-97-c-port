#include "recovered/game_path_prefix_set.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game path-prefix set check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807fff00u;
constexpr std::uint32_t EntrySp = 0x807fffd0u;
constexpr std::uint32_t FrameSp = EntrySp - 0x18u;
constexpr std::uint32_t Source = 0x800247e4u;
constexpr std::uint32_t Destination = 0x800d6dacu;
constexpr std::uint32_t GlobalPointer = 0x800d79c8u;

struct Fixture {
    enum Mode {
        Normal,
        UnknownLength,
        UnknownLastByte,
        UnknownSeparator,
        MutateSavedRegisters,
        UnknownSavedReturn,
        InvalidValueKnown
    } mode = Normal;
    std::size_t refuse_event = 0;
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known = std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}
    };
    Nba97GamePathPrefixSetContext context{{regions,2},100,Source,EntrySp,
        0x800299f0u,0xa0a0a0a0u,GlobalPointer,io,this};
    Nba97GamePathPrefixSetProgress progress{};
    std::vector<Nba97GamePathPrefixSetEvent> events;

    Fixture() {
        stack.fill(0xcd);
        stack_known.fill(1);
        putText(Source,"cdrom:");
        put(GlobalPointer+0x44u,0x5cu,1);
        put(GlobalPointer+0x45u,0,1);
    }
    std::uint8_t* byte(std::uint32_t address) {
        for (auto& region:regions)
            if (address>=region.base &&
                std::uint64_t(address-region.base)<region.size)
                return region.data+(address-region.base);
        check(false);
        return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for (auto& region:regions)
            if (address>=region.base &&
                std::uint64_t(address-region.base)<region.size)
                return region.known ? region.known+(address-region.base) : nullptr;
        check(false);
        return nullptr;
    }
    void put(std::uint32_t address,std::uint32_t value,unsigned width=4) {
        for (unsigned i=0;i<width;++i) {
            *byte(address+i)=std::uint8_t(value>>(i*8));
            if (auto* mark=known(address+i)) *mark=1;
        }
    }
    void putText(std::uint32_t address,const char* text) {
        do { put(address++,std::uint8_t(*text),1); } while (*text++);
    }
    std::uint32_t get(std::uint32_t address,unsigned width=4) {
        std::uint32_t result=0;
        for (unsigned i=0;i<width;++i)
            result|=std::uint32_t(*byte(address+i))<<(i*8);
        return result;
    }
    std::string text(std::uint32_t address) {
        std::string result;
        for (unsigned i=0;i<64;++i) {
            const auto value=*byte(address+i);
            if (!value) return result;
            result.push_back(char(value));
        }
        check(false);
        return {};
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GamePathPrefixSetEvent* event,
        Nba97GamePathPrefixSetValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.events.push_back(*event);
        if (f.refuse_event && f.events.size()==f.refuse_event)
            return 0;
        if (event->kind==NBA97_GAME_PATH_PREFIX_COPY) {
            check(event->entry==0x8009cb6cu && event->argument_count==2 &&
                event->argument[0]==Destination && event->argument[1]==Source);
            for (unsigned i=0;i<64;++i) {
                const auto source=*f.byte(Source+i);
                *f.byte(Destination+i)=source;
                *f.known(Destination+i)=*f.known(Source+i);
                if (!source) break;
            }
            value->word=Destination;
            value->known=1;
            return 1;
        }
        check(event->kind==NBA97_GAME_PATH_PREFIX_LENGTH &&
            event->entry==0x8009cb4cu && event->argument_count==1 &&
            event->argument[0]==Destination);
        unsigned length=0;
        while (*f.byte(Destination+length)) ++length;
        value->word=length;
        value->known=f.mode==UnknownLength?0u:
            f.mode==InvalidValueKnown?2u:1u;
        if (f.mode==UnknownLastByte && length)
            *f.known(Destination+length-1u)=0;
        if (f.mode==UnknownSeparator)
            *f.known(GlobalPointer+0x44u)=0;
        if (f.mode==MutateSavedRegisters) {
            f.put(FrameSp+0x14u,0x55667788u);
            f.put(FrameSp+0x10u,0x12344321u);
        }
        if (f.mode==UnknownSavedReturn)
            for (unsigned i=0;i<4;++i)
                *f.known(FrameSp+0x14u+i)=0;
        return 1;
    }
    int run() { return nba97_game_path_prefix_set(&context,&progress); }
};

void startup_cdrom_path() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        !f.progress.separator_appended);
    check(f.text(Destination)=="cdrom:" && f.progress.copied_length==6 &&
        f.progress.final_length==6);
    check(f.progress.operations==7 && f.progress.accesses==5 &&
        f.progress.reads==3 && f.progress.stores==2 &&
        f.progress.callbacks_completed==2);
    check(f.progress.source_address==Source &&
        f.progress.destination_address==Destination &&
        f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp);
    check(f.progress.restored_return_address==0x800299f0u &&
        f.progress.restored_register_s0==0xa0a0a0a0u &&
        f.progress.return_v0==0x3au && f.progress.return_v0_known);
    check(f.get(FrameSp+0x14u)==0x800299f0u &&
        f.get(FrameSp+0x10u)==0xa0a0a0a0u);
    check(f.events.size()==2 && f.events[0].pc==0x800a35f0u &&
        f.events[0].return_address==0x800a35f8u &&
        f.events[0].stack_pointer==FrameSp &&
        f.events[0].global_pointer==GlobalPointer &&
        f.events[1].pc==0x800a35f8u &&
        f.events[1].return_address==0x800a3600u);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);
}

void separator_branches() {
    Fixture empty;
    empty.putText(Source,"");
    check(empty.run()==NBA97_TEXT_COMPLETE && empty.text(Destination).empty() &&
        !empty.progress.separator_appended && !empty.progress.final_length &&
        empty.progress.return_v0==Destination-1u);
    check(empty.progress.operations==6 && empty.progress.accesses==4 &&
        empty.progress.reads==2 && empty.progress.stores==2);

    Fixture slash;
    slash.putText(Source,"DATA\\");
    check(slash.run()==NBA97_TEXT_COMPLETE && slash.text(Destination)=="DATA\\" &&
        !slash.progress.separator_appended && slash.progress.final_length==5 &&
        slash.progress.return_v0==0x3au);
    check(slash.progress.operations==7 && slash.progress.accesses==5);

    Fixture append;
    append.putText(Source,"DATA");
    check(append.run()==NBA97_TEXT_COMPLETE && append.text(Destination)=="DATA\\" &&
        append.progress.separator_appended && append.progress.copied_length==4 &&
        append.progress.final_length==5 && append.progress.return_v0==0x5cu);
    check(append.progress.operations==11 && append.progress.accesses==9 &&
        append.progress.reads==5 && append.progress.stores==4);
    check(append.get(Destination+4u,1)==0x5cu &&
        append.get(Destination+5u,1)==0);
}

void knownness_and_partial_effects() {
    Fixture unknown_length;
    unknown_length.mode=Fixture::UnknownLength;
    check(unknown_length.run()==NBA97_TEXT_UNKNOWN &&
        !unknown_length.progress.completed &&
        unknown_length.progress.stopped_pc==0x800a3604u &&
        unknown_length.progress.callbacks_completed==2 &&
        unknown_length.text(Destination)=="cdrom:");

    Fixture unknown_last;
    unknown_last.putText(Source,"DATA");
    unknown_last.mode=Fixture::UnknownLastByte;
    check(unknown_last.run()==NBA97_TEXT_UNKNOWN &&
        unknown_last.progress.stopped_pc==0x800a3610u &&
        unknown_last.progress.stopped_address==Destination+3u &&
        unknown_last.progress.reads==1);

    Fixture unknown_separator;
    unknown_separator.putText(Source,"DATA");
    unknown_separator.mode=Fixture::UnknownSeparator;
    check(unknown_separator.run()==NBA97_TEXT_COMPLETE &&
        unknown_separator.progress.completed &&
        unknown_separator.progress.separator_appended &&
        !unknown_separator.progress.return_v0_known);
    check(*unknown_separator.known(Destination+4u)==0 &&
        *unknown_separator.known(Destination+5u)==1);

    Fixture refused_copy;
    refused_copy.refuse_event=1;
    check(refused_copy.run()==NBA97_TEXT_IO_REFUSED &&
        refused_copy.progress.stopped_pc==0x800a35f0u &&
        refused_copy.progress.stopped_entry==0x8009cb6cu &&
        !refused_copy.progress.callbacks_completed &&
        refused_copy.progress.stores==2);

    Fixture refused_length;
    refused_length.refuse_event=2;
    check(refused_length.run()==NBA97_TEXT_IO_REFUSED &&
        refused_length.progress.stopped_pc==0x800a35f8u &&
        refused_length.progress.callbacks_completed==1 &&
        refused_length.text(Destination)=="cdrom:");

    Fixture restored;
    restored.mode=Fixture::MutateSavedRegisters;
    check(restored.run()==NBA97_TEXT_COMPLETE &&
        restored.progress.restored_return_address==0x55667788u &&
        restored.progress.restored_register_s0==0x12344321u);

    Fixture unknown_saved;
    unknown_saved.mode=Fixture::UnknownSavedReturn;
    check(unknown_saved.run()==NBA97_TEXT_UNKNOWN &&
        !unknown_saved.progress.completed &&
        unknown_saved.progress.stopped_pc==0x800a3638u &&
        unknown_saved.progress.stopped_address==FrameSp+0x14u);

    Fixture invalid;
    invalid.mode=Fixture::InvalidValueKnown;
    check(invalid.run()==NBA97_TEXT_ARGUMENT &&
        invalid.progress.stopped_pc==0x800a35f8u &&
        invalid.progress.callbacks_completed==1);
}

void validation_and_memory_failures() {
    {Fixture f;f.context.operation_budget=0;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800a35e0u &&
        f.progress.stopped_address==FrameSp+0x10u &&
        !f.progress.operations && !f.progress.accesses);}
    {Fixture f;f.context.operation_budget=2;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800a35f0u &&
        f.progress.stopped_entry==0x8009cb6cu &&
        f.progress.stores==2 && !f.progress.callbacks_completed);}
    {Fixture f;f.context.stack_pointer=EntrySp+1u;check(
        f.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        f.progress.stopped_pc==0x800a35e0u);}
    {Fixture f;f.context.io=nullptr;check(f.run()==NBA97_TEXT_IO_REFUSED &&
        f.progress.stopped_pc==0x800a35f0u && f.progress.stores==2);}
    {Fixture f;Nba97GameTextRegion overlap[2]={f.regions[0],f.regions[0]};
        f.context.memory={overlap,2};check(f.run()==NBA97_TEXT_ARGUMENT &&
        !f.progress.operations);}
    {Fixture f;f.regions[0].size=0;check(f.run()==NBA97_TEXT_ARGUMENT);}
    {Fixture f;f.regions[0].data=nullptr;check(f.run()==NBA97_TEXT_ARGUMENT);}
    {Fixture f;f.context.memory={nullptr,1};check(f.run()==NBA97_TEXT_ARGUMENT);}
    Nba97GamePathPrefixSetProgress progress{};
    check(nba97_game_path_prefix_set(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_path_prefix_set(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    startup_cdrom_path();
    separator_branches();
    knownness_and_partial_effects();
    validation_and_memory_failures();
    std::printf("game_path_prefix_set: %u checks passed\n",checks);
}
