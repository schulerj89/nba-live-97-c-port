#include "recovered/game_ball_release.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks;
void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"ball release check %u failed\n",checks);std::abort();}}
constexpr uint32_t A=0x80120000,B=A+244,C=A+488,T=0x800b81a0;
struct Store {uint32_t pc,address,value;unsigned width;};
struct Fixture {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(0x200000),known=std::vector<uint8_t>(0x200000,1);
    std::vector<Store> stores;
    Nba97GameTipoffContext context{access,nullptr,this};
    void put(uint32_t a,uint32_t v,unsigned w){a-=0x80000000;for(unsigned i=0;i<w;++i){bytes[a+i]=static_cast<uint8_t>(v>>(8*i));known[a+i]=1;}}
    uint32_t get(uint32_t a,unsigned w){a-=0x80000000;uint32_t v=0;for(unsigned i=0;i<w;++i)v|=uint32_t(bytes[a+i])<<(8*i);return v;}
    void unknown(uint32_t a,unsigned w){for(unsigned i=0;i<w;++i)known[a-0x80000000+i]=0;}
    Fixture(){put(B,1,4);put(0x800fdc48,C,4);put(0x800fdc00,2,2);put(0x800fdc02,65535,2);put(0x800fdc28,32,4);put(0x800fdb90,0x81,2);put(T,20,2);put(C+0x10,0x5c00,4);put(0x8001edee,4,2);}
    static int access(void* p,uint32_t pc,uint32_t a,unsigned w,int write,Nba97GamePeriodValue* v){
        auto& f=*static_cast<Fixture*>(p);if(a<0x80000000||uint64_t(a)+w>0x80200000)return NBA97_TIPOFF_RANGE;
        unsigned count=0;for(unsigned i=0;i<w;++i){auto k=f.known[a-0x80000000+i];if(k>1)return NBA97_TIPOFF_ARGUMENT;count+=k;}
        if(write){f.stores.push_back({pc,a,v->word,w});f.put(a,v->word,w);for(unsigned i=0;i<w;++i)f.known[a-0x80000000+i]=v->known;}
        else {if(count&&count!=w)return NBA97_TIPOFF_UNKNOWN;v->known=static_cast<uint8_t>(count==w);v->word=v->known?f.get(a,w):0;}
        return 1;
    }
};
}
int main(){
    Nba97GameTipoffReceipt r{};
    {
        Fixture f;check(nba97_game_ball_release(&f.context,A,B,&r)==1&&r.completed);
        check(f.get(0x800fdbcc,2)==65535&&f.get(0x800fdbd2,2)==1&&f.get(0x800fdc34,4)==C);
        check(f.get(A+0xb4,2)==30&&f.get(B+0xb6,2)==24);
        check(f.get(C+0x18,2)==static_cast<uint16_t>(-528)&&f.get(0x800fdb90,2)==0x81);
        check(f.stores[f.stores.size()-2].pc==0x800589e4&&f.stores.back().pc==0x80058a44);
    }
    for(int x: {-100000,100000})for(int z: {-50000,50000}){
        Fixture f;f.put(B+8,static_cast<uint32_t>(x),4);f.put(B+12,static_cast<uint32_t>(z),4);
        check(nba97_game_ball_release(&f.context,A,B,&r)==1);
        const int targetx=x<0?-0x15800:0x15800,targetz=z<0?-0xa800:0xa800;
        check(f.get(B+20,2)==static_cast<uint16_t>((targetx-x)/20));
        check(f.get(B+22,2)==static_cast<uint16_t>((targetz-z)/20));
        check(f.get(C+20,2)==static_cast<uint16_t>(targetx/20)&&f.get(C+22,2)==static_cast<uint16_t>(targetz/20));
    }
    {
        Fixture f;f.put(T,0,2);
        check(nba97_game_ball_release(&f.context,A,B,&r)==NBA97_BALL_RELEASE_DIVZERO);
        check(r.stopped_pc==0x80058828&&!r.completed&&f.get(0x800fdbcc,2)==65535&&f.get(0x800fdc34,4)==C);
        check(f.get(B+0xb6,2)==0);
    }
    {
        Fixture f;f.put(T,65535,2);f.put(C+8,0x80000000,4);
        check(nba97_game_ball_release(&f.context,A,B,&r)==NBA97_BALL_RELEASE_DIVOVERFLOW);
        check(r.stopped_pc==0x80058840);
    }
    {
        Fixture f;f.unknown(0x800fdc48,4);
        check(nba97_game_ball_release(&f.context,A,B,&r)==NBA97_TIPOFF_UNKNOWN);
        check(r.stopped_pc==0x80058810&&r.stores==6&&f.known[0xfdc34]==0);
    }
    {
        Fixture f;f.unknown(0x800fe8cc,2);
        check(nba97_game_ball_release(&f.context,A,B,&r)==NBA97_TIPOFF_UNKNOWN);
        check(r.stopped_pc==0x80058674&&r.stores==4&&f.get(0x800fdbd2,2)==1);
    }
    {
        Fixture f;f.unknown(T+2,2);
        check(nba97_game_ball_release(&f.context,A,B,&r)==NBA97_TIPOFF_UNKNOWN);
        check(r.stopped_pc==0x800589e8&&f.known[C+24-0x80000000]==0&&f.stores.back().pc==0x800589e4);
    }
    {
        Fixture f;f.unknown(B,4);
        check(nba97_game_ball_release(&f.context,A,B,&r)==1&&f.known[0xfdbd2]==0);
    }
    {
        Fixture f;f.put(0x800fdc48,T-0x14,4);f.put(T-0x14+8,0,4);f.put(T-0x14+12,0,4);f.put(B+12,4000,4);
        // Ball Z velocity aliases the later vertical table halfword. It is200,
        // not the original negative entry, so the second vertical store runs.
        f.put(T+2,65535,2);
        check(nba97_game_ball_release(&f.context,A,B,&r)==1);
        check(f.stores[f.stores.size()-2].value==200&&f.stores.back().pc==0x80058a44);
    }
    for(uint32_t phase: {0u,127u,128u,129u,130u,65535u}){
        Fixture f;f.put(0x800fdb90,phase,2);f.put(T+2,65535,2);
        check(nba97_game_ball_release(&f.context,A,B,&r)==1);
        check(f.get(0x800fdb90,2)==((phase<128||phase==65535)?0u:phase));
        check(f.get(C+24,2)==65535);
    }
    for(uint32_t kind: {0u,1u,65535u}){
        Fixture f;f.put(0x800fdc02,kind,2);const uint32_t table=(kind==0?0x800b81c8:kind==1?0x800b81b0:0x800b8198)+8;
        f.put(table,20,2);f.put(table+2,65535,2);f.put(B+8,static_cast<uint32_t>(-100),4);
        check(nba97_game_ball_release(&f.context,A,B,&r)==1);
        check(f.get(C+20,2)==static_cast<uint16_t>(kind==0?-7:-5)); // arithmetic -5>>2 ==-2.
    }
    {
        Fixture f;f.put(0x800fe8cc,1,2);f.put(0x800b8198+12,20,2);f.put(0x800b8198+14,65535,2);
        check(nba97_game_ball_release(&f.context,A,B,&r)==1&&f.get(A+0xb4,2)==0);
        f.known[0x21d72]=2; // Unvisited option metadata must not block this route.
        check(nba97_game_ball_release(&f.context,A,B,&r)==1);
    }
    std::printf("game_ball_release: %u checks passed\n",checks);
}
