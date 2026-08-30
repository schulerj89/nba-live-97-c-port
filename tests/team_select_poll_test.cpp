#include "recovered/team_select_poll.h"
#include "recovered/team_select.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
struct Poll {
    Nba97TeamPoll state{};
    Nba97TeamSample sample{};
    std::array<uint16_t,8> raw{};
    unsigned frames=0;
    Poll() {nba97_team_poll_open(&state);}
    int present(bool moving=false) {
        check(nba97_team_poll_prepare(&state,moving)!=0,"source presentation was not requested");
        ++frames;return nba97_team_poll_presented(&state,raw.data(),&sample);
    }
    void finish() {nba97_team_poll_finish_callback(&state,nba97_team_poll_caller_wait(sample.token,sample.delay));}
};
void tests() {
    Poll setup;
    check(nba97_team_poll_setup_start(&setup.state,0) && setup.state.pad.prior_mask==0x80 &&
          setup.state.pad.repeat_counter==2 && setup.state.pad.prior_controller==0,
          "state0 changed Start applies both source history updates");
    setup.raw[0]=0x80;check(!setup.present() && setup.state.phase==NBA97_TEAM_EXIT_CHANGE,"Setup held Start barrier");
    setup.raw[0]=8;check(!setup.present() && setup.state.phase==NBA97_TEAM_EXIT_FINAL,"Setup changed nonzero mask");
    check(setup.present()==NBA97_TEAM_POLL_EXITED,"Setup final cleanup before TeamSelect");
    nba97_team_poll_open(&setup.state);
    check(setup.present()==NBA97_TEAM_POLL_INPUT && setup.sample.token==8 && !setup.state.pad.repeat_counter,
          "changed held key carries into TeamSelect first poll");
    // The source picks the first nonzero physical mask, including entire chords.
    for(unsigned token=1;token<0x4000;++token) {
        Poll p;p.raw[3]=static_cast<uint16_t>(token);p.raw[7]=0x80;
        check(p.present()==NBA97_TEAM_POLL_INPUT && p.sample.token==token && p.sample.controller==3,
              "first physical controller/full mask");
        const unsigned delay=token==4 || token==8 ? 7:4;
        const unsigned wait=token<=8 && (token==1 || token==2 || token==4 || token==8) ? delay:
                            (token&0x3e50 ? 5:0);
        check(p.sample.delay==delay && nba97_team_poll_caller_wait(p.sample.token,p.sample.delay)==wait,
              "source exact-direction and masked-action delay");
        check(!nba97_team_poll_prepare(&p.state,0),"active callback owns its presentations");
    }
    Poll repeat;repeat.raw[0]=8;
    check(repeat.present()==NBA97_TEAM_POLL_INPUT && repeat.state.pad.repeat_counter==0,"initial Left");
    for(unsigned held=1;held<=30;++held) {
        const auto before=repeat.state.pad;const unsigned old_delay=repeat.sample.delay,begin=repeat.frames;
        repeat.finish();
        // A release/repress inside the wait is invisible to the source poll.
        for(unsigned i=0;i<old_delay;++i) {
            repeat.raw[0]=i&1 ? 8:0;
            check(!repeat.present(),"post callback wait polled too soon");
            check(repeat.state.pad.prior_mask==before.prior_mask &&
                  repeat.state.pad.repeat_counter==before.repeat_counter &&
                  repeat.state.pad.prior_controller==before.prior_controller,"post wait changed history");
        }
        repeat.raw[0]=8;
        check(repeat.present()==NBA97_TEAM_POLL_INPUT && repeat.frames-begin==old_delay+1,
              "directional delay plus mandatory poll presentation");
        const unsigned counter=held<24 ? held*2:48;
        const unsigned delay=counter<=15 ? 7:counter<=27 ? 5:counter<=37 ? 3:1;
        check(repeat.state.pad.repeat_counter==counter && repeat.sample.delay==delay,"repeat acceleration thresholds");
    }
    Poll odd;odd.state.pad={8,47,0};odd.raw[0]=8;
    check(odd.present()==NBA97_TEAM_POLL_INPUT && odd.state.pad.repeat_counter==49,"source odd47 becomes49");
    Poll empty;empty.state.pad={8,26,2};
    check(!empty.present() && empty.state.pad.prior_controller==255 && empty.state.pad.prior_mask==8 &&
          empty.state.pad.repeat_counter==26,"empty poll changes only controller sentinel");
    empty.raw[2]=8;check(empty.present()==NBA97_TEAM_POLL_INPUT && !empty.state.pad.repeat_counter,
                        "observed release resets repetition on next nonzero mask");
    Poll settle;settle.raw[0]=4;
    for(unsigned i=0;i<9;++i)check(!settle.present(true),"moving selected text must settle before poll");
    check(settle.present()==NBA97_TEAM_POLL_INPUT && settle.frames==10,"separate poll after settlement");

    // Help returns with the acknowledged shared mask, without resetting repeat.
    Poll help;help.state.pad={0x800,24,0};help.raw[0]=0x20;help.present();
    help.state.pad.prior_mask=0x800;help.state.pad.repeat_counter=24;
    nba97_team_poll_finish_callback(&help.state,0);help.raw[0]=0x800;
    check(help.present()==NBA97_TEAM_POLL_INPUT && help.state.pad.repeat_counter==26,
          "modal acknowledgement history retained until next mandatory poll");
    const auto history=help.state.pad;nba97_team_poll_open(&help.state);
    check(!std::memcmp(&history,&help.state.pad,sizeof(history)),"reentry cannot reset shared history");

    // Start and Select both wait for changed initiating input and then cleanup1.
    for(uint16_t token:{uint16_t(0x80),uint16_t(0x100)}) {
        Poll exit;exit.raw[4]=token;exit.present();
        check(nba97_team_poll_exit(&exit.state)!=0,"exit dispatched");
        for(unsigned i=0;i<3;++i)check(!exit.present() && exit.state.phase==NBA97_TEAM_EXIT_CHANGE,"held exit barrier");
        exit.raw[0]=0x800;check(!exit.present() && exit.state.phase==NBA97_TEAM_EXIT_CHANGE,"other pad cannot release exit");
        exit.raw[4]=token|1;check(!exit.present() && exit.state.phase==NBA97_TEAM_EXIT_FINAL,"changed nonzero mask releases exit");
        check(exit.present()==NBA97_TEAM_POLL_EXITED && exit.state.pad.prior_mask==token &&
              exit.state.pad.prior_controller==4 && !nba97_team_poll_prepare(&exit.state,0),"separate final cleanup presentation");
    }

    // Circle owner78 + caller5 + mandatory poll1. Raw input held in the owner
    // can dispatch at the final poll; it is not discarded as a keydown event.
    Poll random;random.raw[0]=0x40;random.present();
    Nba97TeamSelect team{};check(nba97_team_select_open(&team,3,24,3,24)!=0,"random team fixture");
    Nba97TeamRandom animation{};uint32_t rng[6]={1,2,3,4,5,6};
    check(nba97_team_random_begin(&animation,&team,rng)!=0,"random first mutation before owner presentation");
    unsigned owners=0,choices=1;
    while(nba97_team_random_busy(&animation)) {
        check(!nba97_team_poll_prepare(&random.state,0),"random callback blocks generic poll");
        choices+=nba97_team_random_tick(&animation,&team,rng);++owners;
        if(owners==66)check(choices==12 && animation.wait==12,"last candidate owns12, not18");
    }
    check(owners==78 && choices==12,"Circle owner presentation sum");
    random.raw[0]=0x800;random.finish();
    for(unsigned i=0;i<5;++i)check(!random.present(),"Circle caller wait5");
    check(random.present()==NBA97_TEAM_POLL_INPUT && random.sample.token==0x800 && owners+random.frames-1==84,
          "buffered Cross at source84 boundary");
    std::cout<<"TEAM POLL PASS:16383 masks; first pad, repeat/post/settle, retained history, exit barriers and78+5+1 random\n";
}
}
int main(){try{tests();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
