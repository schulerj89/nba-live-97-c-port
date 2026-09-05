#include "feload_entry_capture.h"
#include "recovered/feload_entry.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace nba97 {
namespace {
// Synthetic fixture access, not an implementation of any PS1 subroutine.
std::uint8_t* locate(const Nba97GameTextMemory& memory,std::uint32_t address,
                     std::size_t width,bool publish_known=false) {
    for(std::size_t i=0;i<memory.count;++i) {
        auto& region=memory.region[i];
        if(address<region.base)continue;
        const auto offset=std::uint64_t(address)-region.base;
        if(offset>region.size || width>region.size-offset)continue;
        if(publish_known && region.known)std::fill_n(region.known+offset,width,std::uint8_t{1});
        return region.data+offset;
    }
    throw std::runtime_error("FELOAD capture fixture escaped retained memory");
}
void put(const Nba97GameTextMemory& memory,std::uint32_t address,std::uint32_t value) {
    auto* p=locate(memory,address,4,true);
    for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(value>>(8*i));
}
struct ChildLog { std::vector<Nba97FeloadEntryEvent> events; };
int child(void* user,const Nba97GameTextMemory*,const Nba97FeloadEntryEvent* event,
          Nba97FeloadEntryRegisters*,Nba97FeloadEntryCalleeOutcome* outcome) {
    auto& log=*static_cast<ChildLog*>(user);
    log.events.push_back(*event);
    // Explicit diagnostic services exercise startup control flow. Neither body
    // is recovered here: no BIOS heap or frontend loader behavior is invented.
    if(event->entry==0x801e1590u && event->pc==0x801e1498u) {
        *outcome=NBA97_FELOAD_ENTRY_CALLEE_RETURNED;
        return 1;
    }
    if(event->entry==0x801e136cu && event->pc==0x801e14acu) {
        *outcome=NBA97_FELOAD_ENTRY_CALLEE_TRANSFERRED;
        return 1;
    }
    return 0;
}
}

bool FeloadEntryCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameMainEvent* event,Nba97GameMainValue* value,
    Nba97GameMainCalleeOutcome* outcome) {
    if(!memory || !event || !value || !outcome || !receipt.empty() ||
       event->kind!=NBA97_GAME_MAIN_INDIRECT_CALL || event->pc!=0x80029ba8u ||
       event->entry!=0x801e1410u)return false;
    // Runtime-generated nonretail initial data. The preceding copy proves the
    // entry header independently; these globals are explicit service fixtures.
    put(*memory,0x801e8b70u,0x00200000u);
    put(*memory,0x801e8b6cu,0x00004000u);
    std::fill_n(locate(*memory,0x801e903cu,0x204cu,true),0x204cu,std::uint8_t{0xa5});
    ChildLog log;
    Nba97FeloadEntryContext context{};
    context.memory=*memory;context.operation_budget=2200;
    context.io=child;context.user=&log;
    auto& registers=context.registers.gpr;
    registers[NBA97_FELOAD_R_SP]={event->stack_pointer,1};
    registers[NBA97_FELOAD_R_GP]={event->global_pointer,1};
    registers[NBA97_FELOAD_R_RA]={event->return_address,1};
    for(unsigned i=0;i<3;++i)
        registers[NBA97_FELOAD_R_S0+i]={event->saved_register[i],1};
    Nba97FeloadEntryProgress progress{};
    const auto result=nba97_feload_entry(&context,&progress);
    bool cleared=true;
    const auto* bss=locate(*memory,0x801e9040u,0x2048u);
    for(unsigned i=0;i<0x2048u;++i)cleared=cleared && bss[i]==0;
    if(result!=NBA97_TEXT_COMPLETE || !progress.completed || !progress.transferred ||
       progress.words_cleared!=2067 || log.events.size()!=2 || !cleared ||
       progress.saved_return_address.word!=event->return_address ||
       progress.restored_return_address.word!=event->return_address)return false;
    const auto& live=progress.registers.gpr;
    *value={live[NBA97_FELOAD_R_V0].word,live[NBA97_FELOAD_R_V0].known};
    *outcome=NBA97_GAME_MAIN_CALLEE_TRANSFERRED;
    std::ostringstream out;
    out<<"{\n  \"program\": \"FELOAD\", \"address\": \"0x801E1410\", "
        "\"inclusive_end\": \"0x801E14B7\", \"bytes\": 168, \"instructions\": 42,\n"
        "  \"instruction_sha256\": \"22bb7caff6b8fd97b13608b31ea7af7515c565dac67a989c418496c1818b0716\",\n"
        "  \"call_pc\": \"0x80029BA8\", \"classification\": \"no direct visual effect\",\n"
        "  \"driver\": \"native recovered-input handlers: Game Setup, Team Select, User Setup\",\n"
        "  \"scope\": \"synthetic globals and explicit child fixtures; no live FELOAD loader or gameplay\",\n"
        "  \"words_cleared\": "<<progress.words_cleared<<", \"bss_before_byte\": 165, "
        "\"bss_after_zero_except_saved_ra\": true,\n"
        "  \"operations\": "<<progress.operations<<", \"reads\": "<<progress.reads<<
        ", \"stores\": "<<progress.stores<<", \"heap_base\": "<<progress.heap_base<<
        ", \"heap_size\": "<<progress.heap_size<<",\n"
        "  \"saved_ra\": "<<progress.saved_return_address.word<<
        ", \"restored_ra\": "<<progress.restored_return_address.word<<
        ", \"sp\": "<<live[NBA97_FELOAD_R_SP].word<<
        ", \"gp\": "<<live[NBA97_FELOAD_R_GP].word<<
        ", \"s8\": "<<live[NBA97_FELOAD_R_S8].word<<",\n"
        "  \"calls\": [";
    for(std::size_t i=0;i<log.events.size();++i) {
        const auto& call=log.events[i];if(i)out<<',';
        out<<"{\"pc\": "<<call.pc<<", \"entry\": "<<call.entry<<
            ", \"a0\": "<<call.registers.gpr[NBA97_FELOAD_R_A0].word<<
            ", \"a1\": "<<call.registers.gpr[NBA97_FELOAD_R_A1].word<<
            ", \"ra\": "<<call.registers.gpr[NBA97_FELOAD_R_RA].word<<'}';
    }
    out<<"],\n  \"status\": \"transferred to diagnostic child 0x801E136C\",\n"
        "  \"routine_capture_frame_numbers\": [0, 1],\n"
        "  \"captures\": [\"feload-entry-before.ppm\", \"feload-entry-after.ppm\"]\n}\n";
    receipt=out.str();
    return true;
}
void FeloadEntryCapture::writeReceipt(const std::filesystem::path& path) const {
    if(receipt.empty() || before!=after)
        throw std::runtime_error("FELOAD startup capture missing or scanout changed");
    std::ofstream out(path);out<<receipt;
    if(!out)throw std::runtime_error("cannot write FELOAD startup receipt");
}
}
