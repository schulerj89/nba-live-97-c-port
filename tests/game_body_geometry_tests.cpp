#include "recovered/game_body_geometry.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks=0;
void check(bool condition){++checks;if(!condition){std::fprintf(stderr,"body geometry check %u failed\n",checks);std::exit(1);}}
using Ref=Nba97GameBodyReference;
void put(std::vector<uint8_t>& b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=static_cast<uint8_t>(v>>(8*i));}
struct Player {std::array<uint32_t,20> header{},xyz{},a{},b{};uint32_t descriptor=0,groups=0;std::array<uint32_t,15> field{};};
struct Fixture {
    std::array<std::vector<uint8_t>,5> data,known;
    std::array<std::vector<Nba97GameBodyCell>,5> cells;
    std::array<Nba97GameBodyBuffer,5> buffers{};
    Nba97GameBodyGeometryInput input{};Nba97GameBodyGeometryProgress out{};
    std::vector<Nba97GameBodyWrite> journal=std::vector<Nba97GameBodyWrite>(50000);
    std::array<Player,5> players{};
    explicit Fixture(uint32_t n=1,uint32_t p=1,uint32_t s=1,uint32_t count_a=3,uint32_t count_b=3){
        data[0].resize(10*0xbcc,0x55);data[1].resize(200000,0xa7);data[2].resize(4,0x42);
        data[3].resize(320);data[4].resize(320);uint32_t cursor=12;
        put(data[1],0,count_a);put(data[1],4,count_b);
        for(unsigned player=0;player<5;++player){auto& q=players[player];
            for(unsigned part=0;part<20;++part){cursor+=12;q.header[part]=cursor;put(data[1],cursor,n);cursor+=16;
                q.xyz[part]=player?players[0].xyz[part]:cursor;if(!player)cursor+=n*24u;
                q.a[part]=cursor;cursor+=n<<5;q.b[part]=cursor;cursor+=n<<5;}
            cursor+=4;q.descriptor=cursor;put(data[1],cursor,p);put(data[1],cursor+4,s);cursor+=0x3c;
            const auto section=[&](unsigned field,uint32_t count,uint32_t stride){q.field[field/4]=cursor;cursor+=count*stride;};
            section(8,count_a,4);section(12,count_b,4);cursor+=4;
            section(0x10,p,12);section(0x14,s,12);cursor+=4;
            section(0x18,p,12);section(0x1c,p,12);section(0x20,s,12);section(0x24,s,12);cursor+=4;
            section(0x28,p,32);section(0x2c,p,32);cursor+=4;section(0x30,s,32);section(0x34,s,32);cursor+=4;
            q.groups=cursor;cursor+=0x64;
            for(unsigned group=0;group<6;++group){put(data[1],q.groups+group*16,n);cursor+=n<<5;cursor+=n<<5;cursor+=4;}
            // Deliberate original quirk: BankB's encoded group disagrees with A.
            for(unsigned secondary=0;secondary<2;++secondary){uint32_t count=secondary?s:p;
                if(count<=10)for(uint32_t i=0;i<count*3;++i){
                    put(data[1],q.field[(secondary?0x20:0x18)/4]+4*i,0x01000000u+31u);
                    put(data[1],q.field[(secondary?0x24:0x1c)/4]+4*i,0xff000000u+32u);
                    put(data[1],q.field[(secondary?0x14:0x10)/4]+4*i,0x40000001u);}}
        }
        data[1].resize(cursor);bind();input.buffers=buffers.data();input.buffer_count=5;
        input.context={0,0,1};input.cursor={1,8,1};input.roots_a={3,0,1};input.roots_b={4,0,1};
        input.count_a_10423c={count_a,1};input.count_b_fc618={count_b,1};input.physical_base_febe0={0,1};
    }
    void bind(){for(unsigned i=0;i<5;++i){known[i].assign(data[i].size(),1);cells[i].resize((data[i].size()+3)/4);
        buffers[i]={data[i].data(),known[i].data(),data[i].size(),cells[i].data(),cells[i].size(),0,1};}}
    int run(size_t capacity=50000){return nba97_game_body_geometry(&input,journal.data(),capacity,&out);}
    Ref ref(unsigned buffer,size_t offset){auto& c=cells[buffer][offset/4];check(c.is_reference==1);return c.reference;}
    void expect(unsigned buffer,size_t offset,unsigned allocation,uint32_t target){auto r=ref(buffer,offset);check(r.known==1&&r.allocation==allocation&&r.offset==target);}
};
void complete(){Fixture f;check(f.run()==NBA97_BODY_OK);check(f.out.completed&&f.out.players_completed==5&&f.out.return_v0==0);
    check(f.out.cursor.allocation==1&&f.out.cursor.offset==f.data[1].size());
    constexpr int parent[]={-1,0,1,2,-1,4,5,6,-1,8,9,10,9,12,13,14,9,16,17,18};
    constexpr unsigned shared[]={1,2,3,5,6,7};
    for(unsigned player=0;player<5;++player){const auto& q=f.players[player];auto ctx=player*0xbccu;
        check(f.data[0][ctx]==6);
        for(unsigned part=0;part<20;++part){auto h=q.header[part];f.expect(0,ctx+part*0x94+0xb0,1,h);
            f.expect(1,h+4,1,f.players[0].xyz[part]);f.expect(1,h+8,1,q.a[part]);f.expect(1,h+12,1,q.b[part]);
            for(unsigned bank=0;bank<2;++bank)f.expect(0,ctx+part*0x94+0xa4+bank*4,parent[part]<0?3+bank:0,
                parent[part]<0?player*32:ctx+static_cast<uint32_t>(parent[part])*0x94+0x64+bank*32);}
        for(unsigned g=0;g<6;++g)f.expect(1,q.groups+g*16+4,1,f.players[0].xyz[shared[g]]);
        f.expect(1,q.field[0x18/4],1,q.a[1]+31);
        // B offset32 is one-past this packet;50768 stores it without dereference.
        f.expect(1,q.field[0x1c/4],1,q.b[1]+32);
        f.expect(1,q.field[0x10/4],1,q.field[8/4]+4);
        f.expect(1,q.field[0x14/4],1,q.field[12/4]+4);
    }
    for(size_t i=0;i<f.out.writes;++i){const auto& e=f.journal[i];if(e.pc==0x80050c38){
        check(e.is_reference==0&&e.word==31);check(f.journal[i+1].pc==0x80050c48);
        check(f.journal[i+2].pc==0x80050c78&&f.journal[i+2].is_reference==1);
        check(f.journal[i+3].pc==0x80050c90&&f.journal[i+3].is_reference==1);}}
}
void boundaries(){
    {Fixture f;check(f.run(0)==NBA97_BODY_JOURNAL_LIMIT);check(f.out.writes==0&&f.out.stopped_pc==0x8005078c&&f.data[0][0]==0x55);}
    {Fixture f;f.known[0][0]=0;f.known[0][3]=2;check(f.run()==NBA97_BODY_ARGUMENT);check(!f.out.writes&&f.data[0][0]==0x55);}
    {Fixture f;f.known[0][0]=0;check(f.run(1)==NBA97_BODY_JOURNAL_LIMIT);check(f.out.writes==1&&f.known[0][0]==1);}
    {Fixture f;auto h=f.players[0].header[0];f.known[1][h]=0;check(f.run()==NBA97_BODY_UNKNOWN);
        check(f.out.writes==4&&f.out.stopped_pc==0x800507c8);f.expect(1,h+4,1,h+16);}
    {Fixture f;auto h=f.players[0].header[0];f.known[1][h]=0;f.known[1][h+3]=2;check(f.run()==NBA97_BODY_ARGUMENT);check(f.out.writes==4);}
    {Fixture f;f.buffers[0].address_mod4_known=0;check(f.run()==NBA97_BODY_ALIGNMENT_UNKNOWN);check(!f.out.writes);}
    {Fixture f;f.input.cursor.offset=9;check(f.run()==NBA97_BODY_ALIGNMENT_TRAP);check(f.out.writes==3&&f.out.stopped_pc==0x800507bc);}
    {Fixture f;f.buffers[1].size=28;check(f.run()==NBA97_BODY_BOUNDS);check(f.out.writes==3&&f.out.stopped_pc==0x800507bc);}
    {Fixture f;f.input.count_b_fc618={0,0};check(f.run()==NBA97_BODY_UNKNOWN);check(f.out.stopped_pc==0x80050868);
        check(f.journal[f.out.writes-1].pc==0x80050858);}
    {Fixture f;f.input.physical_base_febe0={0,0};check(f.run()==NBA97_BODY_UNKNOWN);check(f.out.stopped_pc==0x80050a48);}
    {Fixture f;f.cells[1][f.players[0].header[0]/4]={{3,0,1},1};check(f.run()==NBA97_BODY_ADDRESS_REQUIRED);check(f.out.stopped_pc==0x800507c8);}
    {Fixture f;f.input.roots_a={};check(f.run()==NBA97_BODY_OK);auto r=f.ref(0,0xa4);check(!r.known&&!r.offset&&!r.allocation);}
    {Fixture f;f.cells[0][0].is_reference=2;check(f.run()==NBA97_BODY_ARGUMENT);check(!f.out.writes);}
    {Fixture f;f.known[1][8]=2;f.buffers[3].address_mod4=3;f.known[3][0]=2;
        // Skipped markers and stored root targets are not reached memory reads.
        check(f.run()==NBA97_BODY_OK);}
    {Fixture f;check(f.run()==NBA97_BODY_OK);check(f.run()==NBA97_BODY_ADDRESS_REQUIRED);check(f.out.stopped_pc==0x80050c2c);}
}
void wrapped(){
    {Fixture f(0x20000000u,0x80000000u,0x80000000u,0x40000000u,0x40000000u);
        f.input.physical_base_febe0={0xffffffffu,1};check(f.run()==NBA97_BODY_OK);check(f.out.cursor.offset==f.data[1].size());
        f.expect(0,0xa4,3,0xffffffe0u);f.expect(0,0xbcc+0xa4,3,0);
        for(size_t i=0;i<f.out.writes;++i)check(f.journal[i].pc!=0x80050c38&&f.journal[i].pc!=0x80050d10);}
    {Fixture f(0,0,0,0,0);check(f.run()==NBA97_BODY_OK);check(f.out.cursor.offset==f.data[1].size());}
}
void allocation_alias(){Fixture f;
    const uint32_t start=static_cast<uint32_t>(f.data[0].size());f.data[0].insert(f.data[0].end(),f.data[1].begin(),f.data[1].end());
    f.bind();f.input.cursor={0,start+8,1};f.known[1][0]=2;check(f.run()==NBA97_BODY_OK);
    f.expect(0,0xb0,0,start+f.players[0].header[0]);f.expect(0,start+f.players[1].header[1]+4,0,start+f.players[0].xyz[1]);
}
void partial_leading_cell(){Fixture f;
    f.data[1].insert(f.data[1].begin(),2,0x31);f.bind();f.cells[1].resize((f.data[1].size()+5)/4);
    f.buffers[1].cells=f.cells[1].data();f.buffers[1].cell_count=f.cells[1].size();f.buffers[1].address_mod4=2;
    f.input.cursor.offset+=2;check(f.run()==NBA97_BODY_OK);check(f.out.cursor.offset==f.data[1].size());
    // Original allocation base+2 and reached offset+2 sum to an aligned word.
    auto& c=f.cells[1][(f.players[0].header[0]+4+2+2)/4];check(c.is_reference==1);
    check(c.reference.allocation==1&&c.reference.offset==f.players[0].xyz[0]+2&&c.reference.known==1);
}
void limits(){Fixture reference;check(reference.run()==NBA97_BODY_OK);const auto count=reference.out.writes;
    for(size_t cap=0;cap<count;cap+=17){Fixture f;check(f.run(cap)==NBA97_BODY_JOURNAL_LIMIT);check(f.out.writes==cap);
        for(size_t i=0;i<cap;++i){const auto& a=f.journal[i];const auto& b=reference.journal[i];
            check(a.pc==b.pc&&a.word==b.word&&a.is_reference==b.is_reference&&a.destination.allocation==b.destination.allocation&&a.destination.offset==b.destination.offset&&a.reference.allocation==b.reference.allocation&&a.reference.offset==b.reference.offset&&a.reference.known==b.reference.known);}}
}
}
int main(){complete();boundaries();wrapped();allocation_alias();partial_leading_cell();limits();std::printf("game_body_geometry: %u checks passed\n",checks);}
