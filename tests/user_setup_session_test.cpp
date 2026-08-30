#include "user_setup_session.hpp"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <iostream>
#include <limits>
#include <stdexcept>
using nba97::UserSetupSession;
namespace {
void check(bool ok,const char* reason) {if(!ok) throw std::runtime_error(reason);}
void topologyTests() {
    using nba97::UserSetupSession;
    const std::array<uint8_t,8> neutral{};
    const auto check=[](bool ok,const char* why) {
        if(!ok) throw std::runtime_error(why);
    };
    const auto pair=[&](const UserSetupSession& s,unsigned active,int countdown,const char* why) {
        check(s.topology()==active && s.topologyCountdown()==countdown,why);
    };
    UserSetupSession primed;primed.open(neutral,{},0);primed.setControllers(2,0x11);
    primed.key(0,8,true);primed.primeEntryTopology();primed.primeEntryTopology();
    pair(primed,2,3,"initial render primes exactly one outer observation");
    check(primed.raw(0)==8,"priming must not consume or clear input");
    primed.setControllers(3,0x11);primed.step(0);
    pair(primed,2,3,"first input step consumes primed pass without another topology observation");
    primed.step(0);pair(primed,2,2,"next new outer pass resumes observation");
    UserSetupSession prior;prior.open(neutral,{},0,0x20,4);prior.key(0,0x80,true);prior.step(0);
    check(prior.priorMask()==0x20 && prior.priorController()==4,"accepted global Start preserves shared history");
    UserSetupSession aggregate;aggregate.open(neutral,{},0);aggregate.setControllers(0,0x11);
    aggregate.key(0,0x100,true);aggregate.key(4,0x100,true);aggregate.step(0);
    check(!aggregate.cancelReady(),"dispatcher waits on complete aggregate Select");
    aggregate.key(0,0x100,false);check(!aggregate.cancelReady(),"other controller still holds Select");
    aggregate.key(0,8,true);check(aggregate.cancelReady(),"changed nonzero aggregate releases dispatcher");
    for(uint16_t port0:{uint16_t(0),uint16_t(0x8000),uint16_t(0x8001),uint16_t(0xffff)})
        for(uint16_t port1:{uint16_t(0),uint16_t(0x8000),uint16_t(0x8001),uint16_t(0xffff)}) {
            Nba97UserTopology t{99,-1};
            check(nba97_user_setup_topology_observe(&t,port0,port1) &&
                  t.active==(port0==0x8000)+2*(port1==0x8000) && t.countdown==3,
                  "multitap driver status uses exact8000 equality");
        }
    const auto unchanged_editor=[&](const Nba97UserSetup& before,const Nba97UserSetup& after) {
        check(!std::memcmp(before.side,after.side,sizeof(before.side)) &&
              !std::memcmp(before.assignment,after.assignment,sizeof(before.assignment)) &&
              !std::memcmp(before.profile,after.profile,sizeof(before.profile)) &&
              !std::memcmp(before.alphabet,after.alphabet,sizeof(before.alphabet)) &&
              !std::memcmp(before.cursor,after.cursor,sizeof(before.cursor)) &&
              !std::memcmp(before.existing,after.existing,sizeof(before.existing)) &&
              !std::memcmp(before.draft,after.draft,sizeof(before.draft)),
              "topology observation changed assignment/profile/editor locals before timed visit");
    };

    // Entry sentinel means the first observed topology adopts immediately.
    // Neither setter calls nor rendering/read-only getters are observations.
    for(unsigned observed=0;observed<4;++observed) {
        UserSetupSession s;s.open(neutral,{},0);
        pair(s,99,-1,"source entry resets topology sentinel/countdown");
        for(unsigned repeat=0;repeat<8;++repeat)s.setControllers(observed,0xff);
        pair(s,99,-1,"setControllers must not adopt or consume observations");
        check(s.connected()==0xff,"physical connected mask updates independently");
        check(s.step(0).empty(),"entry priming at open clock must not dispatch input");
        pair(s,observed,3,"first new outer pass must adopt observed topology");
        check(s.step(0).empty(),"matching-active sample has no input action");
        pair(s,observed,3,"matching-active observation resets countdown to three");
    }

    struct Sample {unsigned observed,active;int countdown;};
    const auto sequence=[&](const Sample* samples,size_t count,const char* why) {
        UserSetupSession s;s.open(neutral,{},0);s.setControllers(0,1);s.step(0);
        pair(s,0,3,"sequence initial topology");
        for(size_t i=0;i<count;++i) {
            s.setControllers(samples[i].observed,1);
            check(s.step(0).empty(),"topology observation does not require elapsed input time");
            pair(s,samples[i].active,samples[i].countdown,why);
        }
    };
    const Sample fifth[]={{1,0,2},{1,0,1},{1,0,0},{1,0,-1},{1,1,3}};
    sequence(fifth,std::size(fifth),"fifth differing observation adopts, not fourth");
    const Sample alternating[]={{1,0,2},{2,0,1},{1,0,0},{2,0,-1},{1,1,3}};
    sequence(alternating,std::size(alternating),"differing observations need not agree with one another");
    const Sample reset[]={{1,0,2},{2,0,1},{0,0,3},{1,0,2},{1,0,1},{1,0,0},{1,0,-1},{1,1,3}};
    sequence(reset,std::size(reset),"an observation equal to active restarts debounce");

    // A source rebuild precedes global readiness and the timed cleanup gate.
    // Controller5 becomes topology-excluded but stays joined until a timed pass;
    // exact Start on adoption therefore commits its unchanged assignment too.
    UserSetupSession early;
    const std::array<uint8_t,8> fiveHome{{0,1,1,1,1,1,0,0}};
    early.open(fiveHome,{},0);early.setControllers(0,1);early.step(0);
    const auto before_early=early.state();early.setControllers(1,1);
    for(int expected:{2,1,0,-1}) {
        check(early.step(0).empty(),"closed timed gate dispatches no row input");
        pair(early,0,expected,"pre-adoption observation sequence");
        unchanged_editor(before_early,early.state());
    }
    early.key(0,0x80,true);
    const auto accepted=early.step(0);
    pair(early,1,3,"adoption must occur before the global exact-Start gate");
    check(accepted.size()==1 && accepted[0].event==NBA97_USER_CONFIRMED &&
          early.state().result==6 && early.state().side[5]==2 && early.state().assignment[5]==1,
          "topology adoption must not move exclusion cleanup ahead of readiness");

    // Open an editor solely through the ordinary session input boundary. Five
    // new outer passes fit inside the closed clock gate and must not reset it.
    UserSetupSession editing;editing.open(neutral,{},0);editing.setControllers(0,1);editing.step(0);
    const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._!@?";
    static_assert(sizeof(alphabet)==69,"synthetic alphabet contains68 bytes");
    std::array<char,68> letters{};std::copy_n(alphabet,letters.size(),letters.begin());
    editing.configureEditor(letters,[](const char* text){return int(std::strlen(text))*8;});
    editing.key(0,4,true);editing.step(7);editing.releaseKeys();
    editing.key(0,1,true);editing.step(14);editing.releaseKeys();
    editing.key(0,0x800,true);editing.step(21);editing.releaseKeys();
    check(editing.state().side[0]==2 && editing.state().profile[0]==0 &&
          editing.state().alphabet[0]==0 && !std::strcmp(editing.state().draft[0],"A"),
          "editor-preservation fixture entered through source input");
    const auto before_editor=editing.state();editing.setControllers(1,1);
    for(unsigned i=0;i<5;++i) {
        check(editing.step(22).empty(),"closed clock gate must not process editor rows");
        unchanged_editor(before_editor,editing.state());
    }
    pair(editing,1,3,"editor does not block outer topology observation");

    // Active0 has visual rows physical0,4. Open Help on the fourth differing
    // observation, just before the next NEW pass would adopt1 (rows0,1,2,3,4).
    // Physical4 was disconnected when that pass began, becomes connected during
    // Help, and must be the next processed row when the old pass resumes.
    UserSetupSession h;h.open(neutral,{},0,0x20);h.setControllers(0,0x03);h.step(0);
    h.setControllers(1,0x03);
    for(int clock:{7,14,21})h.step(clock);
    pair(h,0,0,"three differing observations precede suspended Help pass");
    h.key(0,0x20,true);auto actions=h.step(28);
    check(actions.size()==1 && actions[0].event==NBA97_USER_HELP && actions[0].controller==0,
          "Help suspends old physical0,4 row sequence");
    pair(h,0,-1,"fourth observation arms adoption for next new pass");
    h.openHelp({111,70,290,140},0);
    h.setControllers(1,0x13); // physical0,1,4 live;4 joined the hardware during Help.
    h.key(1,4,true);h.key(4,4,true);
    for(unsigned i=0;i<20;++i) {
        h.tickHelp();h.tickPresentation();
        check(h.step(100+int(i)).empty(),"modal cannot run an outer input pass");
        pair(h,0,-1,"Help presentations/steps must not consume debounce");
    }
    check(h.connected()==0x13 && h.help().phase==NBA97_HELP_WAIT_CHANGE,
          "modal preserves initiating acknowledgement and live physical connectivity");
    h.key(0,0x20,false);h.tickHelp();
    check(h.help().phase==NBA97_HELP_READY,"initiating controller releases Help");
    h.key(0,0x800,true);h.tickHelp();
    for(unsigned i=0;i<30;++i)h.tickHelp();
    check(h.help().phase==NBA97_HELP_RETURN_BARRIER && h.step(1000).empty(),
          "Help return barrier still blocks observations");
    pair(h,0,-1,"held return barrier freezes topology");
    h.key(0,0x800,false);h.tickHelp();
    check(h.help().phase==NBA97_HELP_CLOSED,"Help return changed input");
    actions=h.step(1001);
    pair(h,0,-1,"modal continuation must not adopt topology or count another observation");
    check(actions.size()==1 && actions[0].event==NBA97_USER_SIDE && actions[0].controller==4 &&
          h.state().side[4]==2 && h.state().side[1]==1,
          "resume old physical4 using live connectivity, then clear excluded physical1");
    h.step(1002);
    pair(h,1,3,"following fresh outer pass adopts pending topology");
    check(h.state().side[1]==2,"new topology now processes physical1");
    check(h.priorMask()==0x800 && h.priorController()==0,"ordinary rows preserve shared modal history");

    // Warning/delete dialog presentation helpers cannot advance topology either.
    // This synthetic direct modal has no suspended controller pass, so closing it
    // is followed by a fresh observation (unlike the Help case above).
    UserSetupSession dialog;dialog.open(neutral,{},0,0);dialog.setControllers(0,1);dialog.step(0);
    dialog.setControllers(2,1);dialog.openDialog(nba97::UserSetupDialog::Duplicate,{146,95,220,78},0);
    for(unsigned i=0;i<30;++i) {
        dialog.tickDialog();check(dialog.step(100+int(i)).empty(),"dialog blocks outer pass");
        pair(dialog,0,3,"dialog ticks and steps freeze debounce");
    }
    check(dialog.dialogState().modal.phase==NBA97_HELP_WAIT_CHANGE,"synthetic notice waits for acknowledgement");
    dialog.key(0,0x800,true);
    for(unsigned i=0;i<30;++i) {
        dialog.tickDialog();check(dialog.step(200+int(i)).empty(),"shrinking dialog blocks outer pass");
        pair(dialog,0,3,"dialog acknowledgement/shrink cannot consume debounce");
    }
    check(dialog.dialogState().modal.phase==NBA97_HELP_RETURN_BARRIER,"notice holds its return barrier");
    dialog.releaseKeys();check(dialog.tickDialog()&NBA97_RESET_RETURN,"notice returns after changed input");
    dialog.finishDialog();dialog.step(1);
    pair(dialog,0,2,"first fresh pass after synthetic dialog consumes one observation");

    // Ended source owners do not execute further outer passes. Native deferMatch
    // is the one explicit continuation policy and must restart observation.
    for(uint16_t token:{uint16_t(0x80),uint16_t(0x100)}) {
        UserSetupSession done;done.open(neutral,{},0);done.setControllers(0,1);done.step(0);
        done.setControllers(1,1);done.key(0,token,true);const auto event=done.step(0);
        check(event.size()==1 && done.state().result==(token==0x80 ? 6:-1),"terminal fixture");
        check(done.priorMask()==token && done.priorController()==(token==0x80 ? 0:8),
              "Start preserves shared history; Select writes100/controller8");
        pair(done,0,2,"terminal pass observes before returning");
        done.setControllers(2,0x11);
        for(int clock:{0,1,7,100}) {
            check(done.step(clock).empty(),"terminal result cannot restart source owner");
            pair(done,0,2,"terminal results freeze topology countdown");
        }
        if(token==0x80) {
            done.releaseKeys();done.deferMatch();done.step(1);
            pair(done,0,1,"explicit native defer resumes one fresh topology observation");
        }
        done.open(neutral,{},200);
        pair(done,99,-1,"reentry resets topology independently of repeat history");
        done.setControllers(3,0xff);done.step(200);
        pair(done,3,3,"reentry first sample adopts immediately");
    }
}
void tests() {
    topologyTests();
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
