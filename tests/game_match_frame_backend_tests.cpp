#include "game_match_frame.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using U=std::uint32_t;
unsigned checks=0;
void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
struct Fixture {
    // Explicit synthetic retained RAM, not startup or real resource evidence.
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000,0xa5);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x200000,1);
    std::vector<Nba97GameBodyCell> cells=std::vector<Nba97GameBodyCell>(0x80000);
    Nba97GameBodyBuffer buffer{bytes.data(),known.data(),bytes.size(),cells.data(),cells.size(),0,1};
    Nba97PlayerProjectionAddress address{0x80000000,1};
    nba97::GamePlayerFrame player;
    nba97::GameMatchFrame owner{player};
    Nba97MatchFrameProgress progress{};
    std::vector<U> calls;
    U stop=0,status=0x12345601;
    Fixture(){player.buffers=&buffer;player.buffer_count=1;player.addresses=&address;player.address_count=1;
        owner.io=io;owner.user=this;owner.interrupt_status={status,15};put(0x8001ede8,0);put(0x800b729c,384);pose();}
    void put(U a,U v,unsigned n=4){for(unsigned i=0;i<n;++i){bytes[a-0x80000000+i]=std::uint8_t(v>>(8*i));known[a-0x80000000+i]=1;}}
    U get(U a,unsigned n=4)const{U v=0;for(unsigned i=0;i<n;++i)v|=U(bytes[a-0x80000000+i])<<(8*i);return v;}
    void pointer(U a,U target){put(a,0);cells[(a-0x80000000)/4]={{0,target-0x80000000,1},1};}
    static int io(void* user,const Nba97MatchFrameCall* q,Nba97GamePeriodValue*){
        auto& f=*static_cast<Fixture*>(user);f.calls.push_back(q->entry);
        if(q->entry==f.stop)return NBA97_BODY_BOUNDS;
        if(q->entry==0x80048ff4||q->entry==0x8004900c)return NBA97_BODY_ARGUMENT;
        // Required platform/pose/camera services are explicit test fixtures.
        // No native pass may escape into this callback as a successful no-op.
        if(q->entry==0x80099960){for(U i=0;i<q->args[1];++i)f.put(q->args[0]+4*i,0x00ffffff);}
        if(q->entry==0x8004b1a4||q->entry==0x80052914||q->entry==0x80049300||q->entry==0x80049d34||
           q->entry==0x80057f5c||q->entry==0x80058120||q->entry==0x800581c0)return NBA97_BODY_ARGUMENT;
        return NBA97_BODY_OK;
    }
    void pose(){
        put(0x800f0ed8,0x80110000);put(0x800fc654,0x801c0000);
        put(0x8001ec98,0x801d0000);put(0x800170c8,0x801d0040);
        put(0x801d0008,0x80140000);put(0x801d0048,0x80150000);
        for(unsigned i=0;i<10;++i){const U entity=0x801c0000+i*244;
            for(U offset:{0x84u,0x88u,0x8cu,0x8eu,0x90u,0x92u,0x94u,0x96u,0x9au})put(entity+offset,0,2);
            put(entity+0x86,0xffff,2);put(entity+0x8a,0xffff,2);
        }
    }
    void net(){
        pointer(0x80103f44,0x80120000);put(0x800288b4,0x800a46cc);
        for(unsigned i=0;i<30;++i)put(0x80120000+4*i,120);
        std::vector<unsigned> packed{0x10,0xfb,0,6,0x40};
        for(unsigned i=0;i<400;++i){packed.push_back(0xe0);for(unsigned j=0;j<4;++j)packed.push_back(0);}
        packed.push_back(0xfc);for(unsigned i=0;i<packed.size();++i)put(0x80120078+i,packed[i],1);
        for(U a:{0x800b7a00u,0x800b7a04u,0x800b7a08u,0x800b7a0cu,0x800dcf10u})put(a,0);
        put(0x800b72dc,1);put(0x8010b60c,1);put(0x800fcc54,3);
        pointer(0x800fc660,0x80150000);put(0x80150000,0,2);
        for(unsigned i=0;i<8;++i)put(0x800f9fd8+4*i,(i==0||i==2||i==4)?4096:i==7?5000:0);
        for(unsigned i=0;i<4096;++i)put(0x800b3254+4*i,0x10000000);
        for(unsigned i=0;i<15;++i)for(unsigned j=0;j<3;++j)put(0x800b731c+8*i+2*j,0,2);
        for(U a:{0x800fa630u,0x800fa632u,0x800fa634u,0x800fa638u,0x800fa63au,0x800fa63cu,
                 0x800fab98u,0x800fab9au,0x800fab9cu,0x801076e4u,0x801076e6u,0x801076e8u,
                 0x80108a08u,0x80108a0au,0x80108a0cu})put(a,0,2);
        put(0x800fa634,5000,2);player.geometry.root.depth_cue_a={0,1};player.geometry.root.depth_cue_b={0,1};
        owner.average_scale4={1024,1};
        for(unsigned side=0;side<2;++side){
            for(unsigned i=0;i<96+1440+3200;++i)known[0x106444+side*0x1324+i]=0;
            for(unsigned i=0;i<15;++i)known[0x1063cc+side*0x1324+i*8+6]=known[0x1063cc+side*0x1324+i*8+7]=0;
        }
        // Stop at the first actual player read, after the complete native net.
        known[0xb72d4]=0;
    }
};
void controls(){
    Fixture missing;missing.owner.io=nullptr;
    check(missing.owner.run(10000,missing.progress)==NBA97_MATCH_FRAME_IO_REQUIRED,"missing platform callback refuses after native pose");
    check(missing.owner.pose_progress.completed&&missing.progress.stores==3&&missing.progress.stopped_entry==0x80099960,"native pose and interrupt disable precede missing table service");
    check(missing.owner.interrupt_status.word==0x12345600&&missing.owner.interrupt_disable.completions==1,"native disable prefix survives table refusal");
    Fixture unknown;unknown.owner.interrupt_status={0x12345601,14};
    check(unknown.owner.run(10000,unknown.progress)==NBA97_BODY_UNKNOWN&&unknown.progress.stopped_entry==0x80048ff4,"unknown explicit CP0 stops at consumed old Status");
    check(unknown.owner.interrupt_status.word==0x12345600&&unknown.owner.interrupt_status.known_mask==14,"partial CP0 effect remains visible");
    for(U h:{384u,0x8000u,0xffffffffu}){Fixture f;f.stop=0x80075d40;f.put(0x800b729c,h);
        check(f.owner.run(10000,f.progress)==NBA97_BODY_BOUNDS,"stop after actual geometry control writes");
        const auto low=h&65535u;check(f.player.geometry.root.distance.word==(low&0x8000u?low|0xffff0000u:low),"H sign extends register, preserving low16");
        check(f.player.geometry.root.offset_x.word==(256u<<16)&&f.player.geometry.root.offset_y.word==(120u<<16),"same player geometry receives source offsets");
        check(f.cells[0x102924/4].is_reference&&f.cells[0x102924/4].reference.offset==0xf5c50,"driver publishes normalized main table reference");
        check(f.owner.interrupt_status.word==0x12345601&&f.owner.interrupt_status.known_mask==15&&
              f.owner.interrupt_disable.completions==1&&f.owner.interrupt_restore.completions==1,"actual native interrupt pair restored before render");
    }
    Fixture malformed;malformed.player.geometry.root.distance={1,0};
    check(malformed.owner.run(10000,malformed.progress)==NBA97_BODY_ARGUMENT,"malformed geometry metadata refuses at consumed control");
    check(malformed.progress.stopped_entry==0x80056074&&malformed.get(0x8001ede8)==1,"control refusal preserves previous bank changes");
    Fixture zero;check(zero.owner.run(0,zero.progress)==NBA97_BODY_JOURNAL_LIMIT&&zero.calls.empty(),"outer zero bound cannot call a child");
}
void shared_native_passes(){
    Fixture f;f.net();
    check(f.owner.run(10000,f.progress)==NBA97_BODY_UNKNOWN,"complete native net reaches unavailable player state");
    check(f.progress.stopped_entry==0x80052914&&f.owner.last_native_entry==0x80052914,"player owner handles reached call without external fallback");
    check(f.owner.net_progress.completed&&f.owner.net_progress.initializations==1&&f.owner.net_progress.links==286&&f.owner.net_progress.decodes==2,"actual net initialization, packets and advance completed");
    check(f.get(0x800b72dc)==0&&f.get(0x80103fa8)==100,"net writes remain in the same retained memory");
    check(f.cells[0x102924/4].is_reference&&f.cells[0x102924/4].reference.offset==0xf5c50,"net reads normalized frame table pointer");
    check(f.get(0x80103f44)==0&&f.cells[0x103f44/4].is_reference,"resource pointer backing bytes remain deliberately unusable");
    check(f.player.geometry.root.screen[2].known&&f.player.geometry.root.depth[3].known,"net projection FIFOs exported back to player owner");
    check(f.owner.average_scale4.known&&f.owner.average_scale4.word==1024&&!f.player.geometry.average_scale3.known,"ZSF4 transport does not invent ZSF3");
    check(f.owner.pass_progress.stores==0&&!f.progress.completed&&f.progress.submissions==0,"unavailable player read prevents later passes and submission");
    for(unsigned side=0;side<2;++side)for(unsigned i=96;i<100;++i){
        auto a=0x106a44+side*0x1324+i*16;
        check(!f.known[a]&&!f.known[a+8]&&!f.known[a+12],"unused net packet fields remain unknown");
    }
    Fixture partial;partial.net();partial.put(0x800b72dc,0);partial.known[0xf9fec]=0;
    check(partial.owner.run(10000,partial.progress)==NBA97_BODY_UNKNOWN&&partial.progress.stopped_entry==0x8004b1a4,"native net refuses at unavailable translation");
    check(partial.player.geometry.root.vector.rotation[0].known&&partial.player.geometry.root.vector.rotation[0].word==4096,"partial net geometry exports even on refusal");
    check(!partial.player.geometry.root.vector.translation[0].known&&!partial.owner.net_progress.completed,"unreached geometry stays unknown");
    Fixture child_limit;child_limit.net();child_limit.owner.child_operation_budget=0;
    check(child_limit.owner.run(10000,child_limit.progress)==NBA97_BODY_JOURNAL_LIMIT&&child_limit.progress.stopped_entry==0x800530fc,"child bound stops at the first native pose access");
    check(child_limit.owner.pose_progress.operations==0&&child_limit.get(0x8001ede8)==0,"zero child budget prevents pose and later frame mutation");
}
}
int main(){try{controls();shared_native_passes();std::cout<<checks<<" match-frame backend checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
