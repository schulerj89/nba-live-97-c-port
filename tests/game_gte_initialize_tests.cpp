#include "recovered/game_gte_initialize.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,"game GTE-initialize check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Cu2=0x40000000u;
constexpr std::uint32_t WrittenMask=
    (1u<<NBA97_GAME_GTE_OFX)|(1u<<NBA97_GAME_GTE_OFY)|
    (1u<<NBA97_GAME_GTE_H)|(1u<<NBA97_GAME_GTE_DQA)|
    (1u<<NBA97_GAME_GTE_DQB)|(1u<<NBA97_GAME_GTE_ZSF3)|
    (1u<<NBA97_GAME_GTE_ZSF4);

struct Fixture {
    Nba97GameGteInitializeState state{};
    Nba97GameGteInitializeContext context{&state,20};
    Nba97GameGteInitializeProgress progress{};

    Fixture() {
        state.cop0_status={0x10900401u,1};
        for(unsigned i=0;i<32;++i)
            state.control[i]={0xa5000000u+i,1};
    }
    int run() {return nba97_game_gte_initialize(&context,&progress);}
};

void source_projection_defaults() {
    Fixture f;
    const auto before=f.state.control;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.state.cop0_status.word==0x50900401u &&
        f.state.cop0_status.known && f.progress.status_before==0x10900401u &&
        f.progress.status_after==0x50900401u &&
        f.progress.return_v0==0x50900401u &&
        f.progress.return_v0_known);
    check(f.state.control[NBA97_GAME_GTE_ZSF3].word==0x155u &&
        f.state.control[NBA97_GAME_GTE_ZSF4].word==0x100u &&
        f.state.control[NBA97_GAME_GTE_H].word==1000u &&
        f.state.control[NBA97_GAME_GTE_DQA].word==0xffffef9eu &&
        f.state.control[NBA97_GAME_GTE_DQB].word==0x01400000u &&
        f.state.control[NBA97_GAME_GTE_OFX].word==0 &&
        f.state.control[NBA97_GAME_GTE_OFY].word==0);
    for(unsigned i=0;i<32;++i) {
        if(WrittenMask&(1u<<i))
            check(f.state.control[i].known==1);
        else
            check(f.state.control[i].word==before[i].word &&
                f.state.control[i].known==before[i].known);
    }
    check(f.progress.operations==9 && f.progress.reads==1 &&
        f.progress.stores==8 && f.progress.controls_written==7 &&
        f.progress.control_written_mask==WrittenMask);
    check(!f.progress.stopped_pc &&
        f.progress.stopped_target==NBA97_GAME_GTE_TARGET_NONE &&
        !f.progress.stopped_register);
}

void repeated_and_unknown_controls() {
    Fixture repeated;
    repeated.state.cop0_status={0xffffffffu,1};
    check(repeated.run()==NBA97_TEXT_COMPLETE &&
        repeated.state.cop0_status.word==0xffffffffu &&
        repeated.progress.status_before==0xffffffffu &&
        repeated.progress.return_v0==0xffffffffu);
    repeated.state.control[NBA97_GAME_GTE_ZSF3]={0xdeadbeefu,1};
    repeated.state.control[31]={0x81234567u,1};
    check(repeated.run()==NBA97_TEXT_COMPLETE &&
        repeated.state.control[NBA97_GAME_GTE_ZSF3].word==0x155u &&
        repeated.state.control[31].word==0x81234567u);

    Fixture unknown;
    for(auto index:{NBA97_GAME_GTE_OFX,NBA97_GAME_GTE_OFY,
            NBA97_GAME_GTE_H,NBA97_GAME_GTE_DQA,NBA97_GAME_GTE_DQB,
            NBA97_GAME_GTE_ZSF3,NBA97_GAME_GTE_ZSF4})
        unknown.state.control[index]={0,0};
    unknown.state.control[5]={0,0};
    check(unknown.run()==NBA97_TEXT_COMPLETE);
    for(auto index:{NBA97_GAME_GTE_OFX,NBA97_GAME_GTE_OFY,
            NBA97_GAME_GTE_H,NBA97_GAME_GTE_DQA,NBA97_GAME_GTE_DQB,
            NBA97_GAME_GTE_ZSF3,NBA97_GAME_GTE_ZSF4})
        check(unknown.state.control[index].known==1);
    check(!unknown.state.control[5].known && !unknown.state.control[5].word);
}

void every_operation_limit() {
    constexpr std::array<std::uint32_t,9> pcs={0x8005667cu,0x80056688u,
        0x80056694u,0x800566a0u,0x800566acu,0x800566b8u,
        0x800566c4u,0x800566ccu,0x800566d0u};
    constexpr std::array<unsigned,9> indices={12,12,
        NBA97_GAME_GTE_ZSF3,NBA97_GAME_GTE_ZSF4,NBA97_GAME_GTE_H,
        NBA97_GAME_GTE_DQA,NBA97_GAME_GTE_DQB,NBA97_GAME_GTE_OFX,
        NBA97_GAME_GTE_OFY};
    for(std::size_t budget=0;budget<pcs.size();++budget) {
        Fixture f;
        const auto before=f.state;
        f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations==budget && f.progress.stopped_pc==pcs[budget] &&
            f.progress.stopped_register==indices[budget]);
        check(f.progress.stopped_target==(budget<2?
            NBA97_GAME_GTE_TARGET_COP0_STATUS:NBA97_GAME_GTE_TARGET_CONTROL));
        if(!budget)
            check(f.state.cop0_status.word==before.cop0_status.word);
        if(budget<2)
            check(!f.progress.status_after);
        else
            check(f.progress.status_after==0x50900401u);
        if(budget)
            check(f.progress.return_v0==0x50900401u &&
                f.progress.return_v0_known);
        if(budget<2)
            for(unsigned i=0;i<32;++i)
                check(f.state.control[i].word==before.control[i].word);
    }
}

void metadata_and_arguments() {
    Fixture unknown_status;
    unknown_status.state.cop0_status={0,0};
    check(unknown_status.run()==NBA97_TEXT_UNKNOWN &&
        unknown_status.progress.operations==1 && !unknown_status.progress.reads &&
        unknown_status.progress.stopped_pc==0x8005667cu);
    Fixture malformed_status;
    malformed_status.state.cop0_status.known=2;
    check(malformed_status.run()==NBA97_TEXT_ARGUMENT &&
        malformed_status.progress.stopped_pc==0x8005667cu);
    Fixture malformed_control;
    malformed_control.state.control[NBA97_GAME_GTE_H].known=2;
    check(malformed_control.run()==NBA97_TEXT_ARGUMENT &&
        malformed_control.progress.operations==5 &&
        malformed_control.progress.stores==3 &&
        malformed_control.progress.controls_written==2 &&
        malformed_control.progress.stopped_pc==0x800566acu &&
        malformed_control.state.control[NBA97_GAME_GTE_ZSF3].word==0x155u &&
        malformed_control.state.control[NBA97_GAME_GTE_ZSF4].word==0x100u);
    Nba97GameGteInitializeProgress progress{};
    check(nba97_game_gte_initialize(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Nba97GameGteInitializeContext missing{nullptr,9};
    check(nba97_game_gte_initialize(&missing,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_gte_initialize(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    source_projection_defaults();
    repeated_and_unknown_controls();
    every_operation_limit();
    metadata_and_arguments();
    std::printf("game_gte_initialize: %u checks passed\n",checks);
}
