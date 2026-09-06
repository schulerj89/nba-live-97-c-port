#include "recovered/game_tipoff_phase.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value){++checks;if(!value){std::fprintf(stderr,"tipoff check %u failed\n",checks);std::abort();}}
constexpr uint32_t E=0x80100000,B=E+10*244,T=0x80020bec;
struct Store{uint32_t pc,address,value;unsigned width;};
struct Fixture {
    std::vector<uint8_t> data=std::vector<uint8_t>(0x200000),known=std::vector<uint8_t>(0x200000,1);
    std::vector<Store> stores;
    std::vector<Nba97GameTipoffCall> calls;
    uint32_t pending=0,poison=0,redirect=0;
    Nba97GameTipoffContext context{access,call,this};
    void put(uint32_t a,uint32_t v,unsigned w){a-=0x80000000;for(unsigned i=0;i<w;++i){data[a+i]=static_cast<uint8_t>(v>>(i*8));known[a+i]=1;}}
    uint32_t get(uint32_t a,unsigned w){a-=0x80000000;uint32_t v=0;for(unsigned i=0;i<w;++i)v|=uint32_t(data[a+i])<<(i*8);return v;}
    Fixture(){for(unsigned i=0;i<11;++i){put(T+i*4,E+i*244,4);put(E+i*244,i,4);}put(0x800fdbd2,65535,2);put(0x800fdb90,0x81,2);put(0x8001edee,4,2);put(0x800fdc40,0x8001edf4,4);}
    static int access(void* p,uint32_t pc,uint32_t address,unsigned w,int write,Nba97GamePeriodValue* v){
        auto& f=*static_cast<Fixture*>(p);
        if(address<0x80000000||uint64_t(address)+w>0x80200000)return NBA97_TIPOFF_RANGE;
        const auto at=address-0x80000000;unsigned count=0;
        for(unsigned i=0;i<w;++i){if(f.known[at+i]>1)return NBA97_TIPOFF_ARGUMENT;count+=f.known[at+i];}
        if(write){f.stores.push_back({pc,address,v->word,w});f.put(address,v->word,w);for(unsigned i=0;i<w;++i)f.known[at+i]=v->known;}
        else {if(count!=0&&count!=w)return NBA97_TIPOFF_UNKNOWN;v->known=static_cast<uint8_t>(count==w);v->word=v->known?f.get(address,w):0;}
        return NBA97_TIPOFF_OK;
    }
    static int call(void* p,const Nba97GameTipoffCall* call,Nba97GamePeriodValue* v){
        auto& f=*static_cast<Fixture*>(p);f.calls.push_back(*call);
        if(call->owner==f.pending)return 0;
        if(call->owner==0x8005fc88){v->known=1;v->word=0x1234ffff;}
        // Synthetic boundary mutation probes, NOT real callee implementations.
        if(call->owner==0x80058610){
            if(f.redirect)f.put(T+5*4,f.redirect,4);
            if(f.poison)f.known[f.poison-0x80000000]=2;
            f.put(0x800fdc40,0x8001eeb8,4);
        }
        return 1;
    }
};
}
int main(){
    Nba97GameTipoffReceipt r{};
    {
        Fixture f;for(unsigned i=0;i<4;++i)f.known[T+16-0x80000000+i]=0;
        check(nba97_game_tipoff_release(&f.context,E,&r)==NBA97_TIPOFF_UNKNOWN);
        check(r.stopped_pc==0x8005bc94&&r.stores==4&&f.get(0x800fdc00,2)==2&&f.get(0x800fdc02,2)==65535);
    }
    {
        Fixture f;for(unsigned i=0;i<4;++i)f.known[E-0x80000000+i]=0;
        check(nba97_game_tipoff_release(&f.context,E,&r)==1);
        check(f.known[0xfdbce]==0&&f.known[0xfdbcf]==0&&f.calls.size()==1);
    }
    for(uint32_t side: {0u,1u,255u})for(uint32_t seed: {0u,4u,0x4000u}){
        Fixture f;f.put(E+0xd9,side,1);f.put(0x8001edee,seed,2);
        uint32_t rng=seed?seed:0xa5a5;rng=((rng<<1)^((rng&0x4000)?0x1d87:0))&65535;
        const uint32_t index=side+3+((rng&8)?1:0);
        f.put(T+index*4,E+2*244,4);
        check(nba97_game_tipoff_release(&f.context,E,&r)==1);
        check(f.calls[0].owner==0x80058610&&f.calls[0].argument[1]==E+2*244);
        check(f.get(0x8001edee,2)==rng);
    }
    {
        Fixture f;f.put(E+0x9a,1,2);f.put(E+0x10,0x7fffffff,4);
        f.put(0x800fed20,65535,2);f.put(0x800fed22,1,2);f.put(0x800fed24,0x8000,2);
        Nba97GamePeriodValue p[3]{};
        check(nba97_game_tipoff_hand(&f.context,E,1,p,&r)==1);
        check(p[0].word==0xffffffe0&&p[1].word==0xfff00000&&p[2].word==0x8000001f);
        f.known[0xfed22]=0;
        p[0]={77,1};p[1]={88,1};p[2]={99,1};
        check(nba97_game_tipoff_hand(&f.context,E,1,p,&r)==NBA97_TIPOFF_UNKNOWN);
        check(p[0].word==0xffffffe0&&p[1].word==0xfff00000&&p[2].word==99);
        check(nba97_game_tipoff_contact(&f.context,B,E,1,1,&r)==NBA97_TIPOFF_UNKNOWN);
        check(f.get(0x800fdc30,2)==1&&r.stores==1);
    }
    for(uint32_t mode: {0u,1u})for(int32_t dx: {-2241,-2240,-1793,-1792,0,1792,1793,2240,2241}){
        Fixture f;const uint32_t p[3]={static_cast<uint32_t>(dx),0,0};
        check(nba97_game_tipoff_hand_contact(&f.context,B,E,p,mode,&r)==1);
        const auto limit=mode?2240:1792;
        check(r.return_v0==((dx>=-limit&&dx<=limit)?(mode?3u:UINT32_MAX):0u));
    }
    {
        Fixture f;check(nba97_game_tipoff_body_contact(&f.context,B,E,56,8,1,&r)==1&&r.return_v0==2&&r.stores==1);
        check(nba97_game_tipoff_body_contact(&f.context,B,E,57,8,1,&r)==1&&r.return_v0==0&&r.stores==0);
        check(nba97_game_tipoff_body_contact(&f.context,B,E,28,7,0,&r)==1&&r.return_v0==UINT32_MAX&&r.stores==1);
        check(nba97_game_tipoff_body_contact(&f.context,B,E,27,7,0,&r)==1&&r.return_v0==0&&r.stores==1);
        f.put(0x800fe8aa,1,2);
        check(nba97_game_tipoff_body_contact(&f.context,B,E,56,17,0,&r)==1&&r.return_v0==2);
        check(nba97_game_tipoff_hand_contact(&f.context,B+1,E,std::vector<uint32_t>(3).data(),1,&r)==NBA97_TIPOFF_ALIGNMENT);
    }
    std::printf("game_tipoff_phase: %u checks passed\n",checks);
}
