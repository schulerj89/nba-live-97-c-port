#include "user_setup_session.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using nba97::UserSetupSession;
namespace {
void check(bool ok,const char* reason) {if(!ok) throw std::runtime_error(reason);}
void tests() {
    const std::array<uint8_t,8> neutral{};
    std::vector<nba97::UserProfile> profiles(3);
    for(unsigned i=0;i<3;++i) {profiles[i].id=i+1;profiles[i].name=std::string(1,char('A'+i));}
    UserSetupSession s;s.open(neutral,profiles,0);
    check(s.profileIds()[2]==3 && s.names().name[1][0]=='B',"profile import");
    profiles.erase(profiles.begin()+1);
    s.open(neutral,profiles,0);
    check(s.profileIds()[1]==0 && s.profileIds()[2]==3,"stable slots retain deletion holes");
    const auto ids=s.profileIds();profiles.push_back(profiles.front());
    bool rejected=false;try {s.open(neutral,profiles,0);} catch(const std::runtime_error&) {rejected=true;}
    check(rejected && s.profileIds()==ids,"duplicate IDs rejected without import mutation");
    s.key(0,4,true);
    check(s.step(6).empty() && s.state().side[0]==1,"controller gate uses >6 ticks");
    auto actions=s.step(7);
    check(actions.size()==1 && s.state().side[0]==2,"controller gate seventh tick");
    s.key(0,4,false);s.step(14);s.key(0,0x80,true);
    actions=s.step(15);
    check(actions.size()==1 && actions[0].event==NBA97_USER_CONFIRMED && s.state().assignment[0]==1,
          "global readiness is not controller-gated");
    s.open(neutral,{},20);
    check(s.state().side[0]==2 && s.state().assignment[0]==1,"entry retains committed assignments");
    s.key(0,8,true);s.step(27);s.key(0,8,false);s.step(34);s.key(0,0x100,true);s.step(35);
    check(s.state().result==-1 && s.state().assignment[0]==1,"cancel retains accepted sides");
    s.open(neutral,{},40);check(s.state().side[0]==2 && s.state().profile[0]==-2,"cancel reentry");

    // Original source visits slot0 before it notices disconnected home slots.
    UserSetupSession capacity;
    const std::array<uint8_t,8> fiveHome{{0,1,1,1,1,1,0,0}};
    capacity.open(fiveHome,{},0);capacity.setControllers(1,1);capacity.key(0,4,true);
    actions=capacity.step(7);
    check(actions.size()==1 && actions[0].event==NBA97_USER_CAPACITY && capacity.state().side[0]==1,
          "capacity observes pre-cleanup later and topology-excluded slots");
    capacity.step(8); // Resume the remainder of the blocked pass.
    for(unsigned i=1;i<8;++i) check(capacity.state().side[i]==1,"post-action disconnect cleanup");

    UserSetupSession wrap;wrap.open(neutral,{},std::numeric_limits<int32_t>::max()-3);wrap.key(0,4,true);
    check(wrap.step(std::numeric_limits<int32_t>::min()+3).size()==1,"timed gate signed SUBU wrap");
    UserSetupSession backwards;backwards.open(neutral,{},std::numeric_limits<int32_t>::min());backwards.key(0,4,true);
    check(backwards.step(std::numeric_limits<int32_t>::max()).empty(),"negative wrapped elapsed must not pass");

    UserSetupSession h;h.open(neutral,{},0,0x20);h.setControllers(0,0x11);h.key(0,0x20,true);
    actions=h.step(7);check(actions.size()==1 && actions[0].event==NBA97_USER_HELP,"Help source controller");
    h.openHelp({111,70,290,140},0);
    for(unsigned i=0;i<20;++i) h.tickHelp();
    h.key(4,0x800,true);h.tickHelp();
    check(h.help().phase==NBA97_HELP_WAIT_CHANGE,"other controller cannot acknowledge Help");
    h.key(0,0x20,false);h.tickHelp();
    check(h.help().phase==NBA97_HELP_READY,"Help invoking controller release");
    h.key(0,0x100,true);h.tickHelp();
    check(h.step(100).empty() && h.state().result==0,"Help consumes Select");
    for(unsigned i=0;i<20;++i) h.tickHelp();
    h.key(0,0x100,false);h.tickHelp();h.key(4,0x800,false);
    check(h.help().phase==NBA97_HELP_CLOSED,"Help release barrier");
    h.step(101);check(h.state().result==0,"resume pass without leaking modal input");
    h.key(0,4,true);h.key(0,0x80,true);h.step(110);
    check(h.state().result==0,"raw Start+direction chord cannot confirm");
    h.releaseKeys();check(h.raw(0)==0 && h.raw(4)==0,"focus loss clears keyboard masks");
    UserSetupSession repeat;repeat.open(neutral,{},0);repeat.key(0,8,true);repeat.step(7);
    repeat.open(neutral,{},8);repeat.key(0,8,true);
    check(repeat.step(15).empty() && repeat.state().side[0]==1,"shared repeat history retained on reentry");

    UserSetupSession fixed;
    profiles.resize(3);
    for(unsigned i=0;i<3;++i) {profiles[i].id=i+1;profiles[i].name=std::string(1,char('A'+i));}
    profiles[0].slot=1;profiles[1].slot=7;profiles[2].slot=19;
    fixed.open(neutral,profiles,0);
    check(fixed.profileIds()[1]==1 && fixed.profileIds()[7]==2 && fixed.profileIds()[19]==3 &&
          fixed.profileIds()[0]==0,"v2 persisted holes preserved");
    // State5 inherits context+724, rather than recording Triangle before Help.
    UserSetupSession inherited;inherited.open(neutral,{},0,0x80);
    inherited.key(0,0x20,true);inherited.step(7);inherited.openHelp({111,70,290,140},0);
    for(unsigned i=0;i<50;++i) inherited.tickHelp();
    check(inherited.help().phase==NBA97_HELP_RETURN_BARRIER,
          "different held trigger dismisses after growth without release");
    inherited.releaseKeys();inherited.tickHelp();
    check(inherited.help().phase==NBA97_HELP_CLOSED,"inherited Help return barrier");
    UserSetupSession notice;notice.open(neutral,{},0,0);
    notice.openDialog(nba97::UserSetupDialog::Duplicate,{146,95,220,78},0);
    for(unsigned i=0;i<30;++i)notice.tickDialog();
    check(notice.dialogState().modal.phase==NBA97_HELP_WAIT_CHANGE,"prior zero waits for change");
    notice.key(0,0x200,true);
    int sounds=0;for(unsigned i=0;i<30;++i)sounds+=bool(notice.tickDialog()&32);
    check(sounds==1 && notice.dialogState().modal.phase==NBA97_HELP_RETURN_BARRIER,
          "notice accepts unnamed nonzero mask and closes once");
    notice.releaseKeys();check(notice.tickDialog()&NBA97_RESET_RETURN,"notice return waits for mask change");
    notice.finishDialog();
    UserSetupSession deletion;deletion.open(neutral,{},0,0x80);
    deletion.openDialog(nba97::UserSetupDialog::Delete,{166,88,180,85},0,0);
    for(unsigned i=0;i<30;++i)deletion.tickDialog();
    check(deletion.dialogState().choice==1,"original default cancel");
    for(uint16_t token:{uint16_t(0x80),uint16_t(0x100)}) {
        deletion.key(0,token,true);
        for(unsigned i=0;i<9;++i)check(!(deletion.tickDialog()&NBA97_RESET_CHOSEN),"Start/Select cannot confirm");
        deletion.releaseKeys();for(unsigned i=0;i<8;++i)deletion.tickDialog();
    }
    deletion.key(0,1,true);check(deletion.tickDialog()&NBA97_RESET_UP,"delete selection");
    deletion.releaseKeys();for(unsigned i=0;i<8;++i)deletion.tickDialog();
    deletion.key(0,0x800,true);check(!deletion.tickDialog(),"Cross begins eight presentations");
    for(unsigned i=0;i<7;++i) {
        check(!deletion.tickDialog() && nba97_help_text_visible(&deletion.dialogState().modal),
              "choice text remains during Cross delay");
    }
    const auto close=deletion.tickDialog();
    check((close&NBA97_RESET_CHOSEN) && (close&32) &&
          deletion.dialogState().modal.phase==NBA97_HELP_SHRINKING,"eighth presentation confirms then closes");
    for(unsigned i=0;i<30;++i)deletion.tickDialog();
    check(deletion.dialogState().modal.phase==NBA97_HELP_RETURN_BARRIER,"delete final barrier");
    deletion.releaseKeys();check(deletion.tickDialog()&NBA97_RESET_RETURN,"delete returns after changed input");
}
}
int main() {try {tests();std::cout<<"USER SETUP SESSION PASS\n";return 0;}
catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
