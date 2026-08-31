#include "recovered/game_ball_attachment.h"
#include "game_player_frame.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks;
void check(bool condition){++checks;if(!condition){std::fprintf(stderr,"ball attachment check %u failed\n",checks);std::abort();}}
constexpr uint32_t A=0x80120000,B=0x80121000,P=0x800fed20,Q=0x800faa04,AN=0x80122000,O=0x80123000;
struct Event {uint32_t pc,address,word;unsigned width,kind;};
struct Fixture {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(0x200000),known=std::vector<uint8_t>(0x200000,1);
    std::vector<Event> events;
    Nba97PlayerFrameContext context{access,nullptr,nullptr,this,1000};
    Nba97PlayerFrameProgress progress{};Nba97GamePeriodValue result{};
    void put(uint32_t a,uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i){bytes[a-0x80000000+i]=static_cast<uint8_t>(v>>(8*i));known[a-0x80000000+i]=1;}}
    uint32_t get(uint32_t a,unsigned n=4)const{uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(bytes[a-0x80000000+i])<<(8*i);return v;}
    void hide(uint32_t a,unsigned n){for(unsigned i=0;i<n;++i)known[a-0x80000000+i]=0;}
    Fixture(){put(0x80020bec,A);put(0x800fdc48,B);put(0x8001ecec,AN);put(AN+7,32,1);
        put(A+8,1000);put(A+12,2000);put(A+16,3000);put(B+16,7777);
        put(P,10,2);put(P+2,20,2);put(P+4,30,2);
        put(Q,110,2);put(Q+2,120,2);put(Q+4,130,2);}
    int attach(uint32_t entry){return nba97_game_ball_attachment(&context,entry,&result,&progress);}
    int endpoint(uint32_t hand,uint32_t x=O,uint32_t z=O+4,uint32_t y=O+8){return nba97_game_hand_endpoint(&context,A,hand,x,z,y,&result,&progress);}
    static int access(void* p,uint32_t pc,uint32_t a,unsigned n,unsigned kind,Nba97PlayerFrameValue* v){
        auto& f=*static_cast<Fixture*>(p);if(a<0x80000000||uint64_t(a)+n>0x80200000)return NBA97_BODY_BOUNDS;
        const auto off=a-0x80000000;
        for(unsigned i=0;i<n;++i)if(f.known[off+i]>1)return NBA97_BODY_ARGUMENT;
        if(kind){f.put(a,v->word,n);for(unsigned i=0;i<n;++i)f.known[off+i]=static_cast<uint8_t>((v->known_mask>>i)&1);}
        else {for(unsigned i=0;i<n;++i)if(f.known[off+i]){v->word|=uint32_t(f.bytes[off+i])<<(8*i);v->known_mask|=static_cast<uint8_t>(1u<<i);}}
        f.events.push_back({pc,a,v->word,n,kind});return NBA97_BODY_OK;
    }
};
}
int main(){
    for(uint32_t entry:{uint32_t(NBA97_BALL_ATTACH_BLEND),uint32_t(NBA97_BALL_ATTACH_PRIMARY),uint32_t(NBA97_BALL_ATTACH_SECONDARY)}){
        Fixture f;check(f.attach(entry)==1&&f.progress.completed&&f.result.known);
        bool second=entry==NBA97_BALL_ATTACH_SECONDARY,blend=entry==NBA97_BALL_ATTACH_BLEND;
        check(f.get(B+8)==1000u+(second?110u:10u)*32);
        check(f.get(B+12)==2000u+(second?130u:30u)*32);
        check(f.get(B+16)==(blend?7777u:3000u+(second?120u:20u)*32));
        check(f.get(0x800fdc32,2)==(blend?1u:second?3u:2u));
        check(f.result.word==(blend?3640u:second?3u:2u));
        check(f.progress.stores==(blend?3u:4u)&&f.progress.child_calls==1&&f.progress.math_calls==0);
        for(uint32_t owner:{0x8000u,0xfffeu,0xffffu}){
            Fixture skipped;skipped.put(0x800fdbcc,owner,2);skipped.hide(0x800fdc48,4);skipped.hide(0x80020bec,4);
            check(skipped.attach(entry)==1&&skipped.events.size()==1&&skipped.progress.stores==0);
            check(skipped.result.word==(blend?32u:owner|0xffff0000u));
        }
        // Unknown pointer is read before endpoint lookup, but consumed only
        // after lookup and actor X. All those observations must still occur.
        Fixture missing;missing.hide(0x800fdc48,4);check(missing.attach(entry)==NBA97_BODY_UNKNOWN);
        check(missing.progress.stores==0&&missing.progress.child_calls==1);
        check(missing.events.back().address==A+8&&!missing.result.known);
        // X output aliases actor Z, making the following live Z read change.
        Fixture alias;alias.put(0x800fdc48,A+4);check(alias.attach(entry)==1);
        check(alias.get(A+16)==1000u+(second?110u:10u)*32+(second?130u:30u)*32);
        // Every operation cutoff retains an exact prefix of visible stores.
        for(std::size_t budget=0;budget<f.progress.operations;++budget){
            Fixture bounded;bounded.context.operation_budget=budget;
            check(bounded.attach(entry)==NBA97_BODY_JOURNAL_LIMIT&&!bounded.progress.completed);
            check(bounded.progress.operations==budget&&bounded.events.size()<=f.events.size());
            for(std::size_t i=0;i<bounded.events.size();++i){const auto& a=bounded.events[i];const auto& b=f.events[i];check(a.pc==b.pc&&a.address==b.address&&a.word==b.word&&a.width==b.width&&a.kind==b.kind);}
        }
    }
    for(uint32_t flag:{0u,1u,2u,3u,65535u})for(uint32_t hand:{0u,1u,2u,0xffffffffu}){
        Fixture f;f.put(A+0x9a,flag,2);check(f.endpoint(hand)==1);bool primary=hand==(flag&1);
        check(f.get(O)==(primary?10u:110u)*32&&f.get(O+4)==(primary?30u:130u)*32);
        check(f.get(O+8)==3000u+(primary?20u:120u)*32&&f.result.word==f.get(O+8));
    }
    {
        Fixture f;f.hide(A+0x9b,1);check(f.endpoint(0)==1);
        Fixture bad;bad.known[A+0x9b-0x80000000]=2;check(bad.endpoint(0)==NBA97_BODY_ARGUMENT);
        Fixture needed;needed.hide(A+0x9a,1);check(needed.endpoint(0)==NBA97_BODY_UNKNOWN);
    }
    {
        Fixture f;check(f.endpoint(0,P+4,A+16,O)==1);
        check(f.get(P+4)==320&&f.get(A+16)==10240&&f.get(O)==10880);
        check(f.progress.stores==3); // X changes later Z; Z changes height base.
    }
    {
        Fixture f;f.put(P,0x8000,2);f.put(P+2,0xffff,2);f.put(P+4,0x7fff,2);f.put(A+16,8);
        check(f.endpoint(0)==1&&f.get(O)==0xfff00000&&f.get(O+4)==0x000fffe0&&f.result.word==0xffffffe8);
        Fixture wrapped;wrapped.put(A,0x20000000);check(wrapped.endpoint(0)==1&&wrapped.get(O)==320);
    }
    for(uint32_t state:{20u,21u,22u,23u,0xffffu})for(uint32_t direction:{0u,1u,2u,0xffffu}){
        Fixture f;f.put(A+0x46,state,2);f.put(A+0xb8,direction,2);f.put(A+0x50,20,2);
        check(f.attach(NBA97_BALL_ATTACH_BLEND)==1);
        const bool blend=state==21||state==22;const uint32_t extra=blend?(direction==1?800u:2200u):0;
        check(f.get(B+8)==1320+extra&&f.get(B+12)==2960+extra);
        check(f.get(B+16)==7777&&f.result.word==3640&&f.progress.child_calls==(blend?2u:1u));
    }
    {
        Fixture f;f.put(A+0x46,21,2);f.put(A+0xb8,1,2);f.put(A+0x50,17,2);f.put(AN+7,16,1);
        check(f.attach(NBA97_BALL_ATTACH_BLEND)==NBA97_FRAME_ARITHMETIC_TRAP&&f.progress.stopped_pc==0x80058058&&f.progress.stores==0);
        // Wrapped low32 numerator INT_MIN divided by -1 reaches original BREAK6.
        Fixture overflow;overflow.put(A+0x46,21,2);overflow.put(A+0xb8,1,2);overflow.put(A+0x50,2064,2);overflow.put(AN+7,15,1);
        overflow.put(P,0,2);overflow.put(Q,0x8000,2);
        check(overflow.attach(NBA97_BALL_ATTACH_BLEND)==NBA97_FRAME_ARITHMETIC_TRAP&&overflow.progress.stopped_pc==0x80058070);
        Fixture zoverflow;zoverflow.put(A+0x46,22,2);zoverflow.put(A+0xb8,1,2);zoverflow.put(A+0x50,2064,2);zoverflow.put(AN+7,15,1);
        zoverflow.put(P+4,0,2);zoverflow.put(Q+4,0x8000,2);
        check(zoverflow.attach(NBA97_BALL_ATTACH_BLEND)==NBA97_FRAME_ARITHMETIC_TRAP&&zoverflow.progress.stopped_pc==0x800580bc);
    }
    {
        Fixture f;f.hide(P+2,1);check(f.endpoint(0)==NBA97_BODY_UNKNOWN&&f.progress.stores==2&&f.get(O)==320&&f.get(O+4)==960);
        Fixture align;check(align.endpoint(0,O+2)==NBA97_BODY_ALIGNMENT_TRAP&&align.progress.stopped_pc==0x8002d3c4&&align.progress.stores==0);
        Fixture bounds;bounds.put(A,0x100000);check(bounds.endpoint(0)==NBA97_BODY_BOUNDS);
        Fixture invalid;check(invalid.attach(0)==NBA97_BODY_ARGUMENT&&invalid.events.empty());
    }
    for(uint32_t entry:{NBA97_BALL_ATTACH_BLEND,NBA97_BALL_ATTACH_PRIMARY,NBA97_BALL_ATTACH_SECONDARY}){
        Fixture f;std::vector<Nba97GameBodyCell> cells(f.bytes.size()/4);
        for(uint32_t address:{0x80020becu,0x800fdc48u,0x8001ececu}){
            const auto value=f.get(address);cells[(address-0x80000000)/4]={{0,value-0x80000000,1},1};
            f.put(address,0xcccccccc);f.hide(address,4); // unusable serialized pointer bytes
        }
        Nba97GameBodyBuffer buffer{f.bytes.data(),f.known.data(),f.bytes.size(),cells.data(),cells.size(),0,1};
        Nba97PlayerProjectionAddress address{0x80000000,1};nba97::GamePlayerFrame adapter;
        adapter.buffers=&buffer;adapter.buffer_count=1;adapter.addresses=&address;adapter.address_count=1;
        check(adapter.attachment(entry,1000,f.result,f.progress)==1&&f.progress.completed);
        check(f.get(B+8)==(entry==NBA97_BALL_ATTACH_SECONDARY?4520u:1320u));
        check(f.get(B+16)==(entry==NBA97_BALL_ATTACH_BLEND?7777u:entry==NBA97_BALL_ATTACH_SECONDARY?6840u:3640u));
        check(cells[(0x800fdc48u-0x80000000)/4].is_reference&&f.get(0x800fdc48)==0xcccccccc);
        adapter.address_count=0;f.result={123,1};f.progress.completed=1;
        check(adapter.attachment(entry,1000,f.result,f.progress)==NBA97_BODY_ARGUMENT&&!f.result.known&&!f.progress.completed);
    }
    std::printf("game_ball_attachment: %u checks passed\n",checks);
}
