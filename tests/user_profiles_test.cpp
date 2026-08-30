#include "user_profiles.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
using Bytes=std::vector<uint8_t>;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
void put(Bytes& b,size_t at,uint64_t v,unsigned n=4) {
    for(unsigned i=0;i<n;++i) b.at(at+i)=uint8_t(v>>(i*8));
}
void crc(Bytes& b) {
    put(b,28,0);uint32_t c=0xffffffff;
    for(auto v:b) {c^=v;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}
    put(b,28,static_cast<uint32_t>(~c));
}
Bytes read(const std::filesystem::path& p) {
    std::ifstream in(p,std::ios::binary);return Bytes(std::istreambuf_iterator<char>(in),{});
}
void write(const std::filesystem::path& p,const Bytes& b) {
    std::ofstream out(p,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
    if(!out) throw std::runtime_error("fixture write failed");
}
// Independent wire fixture. PROF order differs from STAT and CTRL order.
Bytes fixture(unsigned version=1) {
    const unsigned sections=version==1 ? 2:3,dir=40,prof=40+16*sections,stat=prof+3*48,ctrl=stat+3*72;
    Bytes b(version==1 ? ctrl:ctrl+3*72);
    const char magic[]="N97PROF";std::copy_n(magic,7,b.begin());
    put(b,8,version,2);put(b,12,41,8);put(b,20,sections);put(b,24,b.size());
    auto section=[&](unsigned at,const char* tag,unsigned offset,unsigned stride) {
        std::copy_n(tag,4,b.begin()+at);put(b,at+4,offset);put(b,at+8,3*stride);put(b,at+12,3);
    };
    section(dir,"PROF",prof,48);section(dir+16,"STAT",stat,72);
    if(version==2) section(dir+32,"CTRL",ctrl,72);
    const char* names[]{"LEGACY USER","SECOND","THIRD"};
    const unsigned slots[]{2,7,19},order[]{2,0,1};
    for(unsigned i=0;i<3;++i) {
        const unsigned p=prof+i*48;const std::string name=names[i];
        put(b,p,100+i,8);put(b,p+8,1000+i,8);put(b,p+16,2000+i,8);
        b[p+24]=uint8_t(name.size());std::copy(name.begin(),name.end(),b.begin()+p+25);
        const unsigned which=order[i],s=stat+i*72;put(b,s,100+which,8);
        for(unsigned field=0;field<16;++field) put(b,s+8+field*4,0x80000000u+which*100+field);
        if(version==2) {
            const unsigned c=ctrl+i*72;put(b,c,100+which,8);b[c+8]=uint8_t(slots[which]);b[c+9]=uint8_t(2+which);
            for(unsigned n=0;n<59;++n)b[c+10+n]=uint8_t(which*60+n);
        }
    }
    crc(b);return b;
}
bool sameStats(const nba97::UserCareerStats& a,const nba97::UserCareerStats& b) {
    return a.games==b.games && a.wins==b.wins && a.losses==b.losses && a.points==b.points &&
        a.field_goals_made==b.field_goals_made && a.field_goals_attempted==b.field_goals_attempted &&
        a.three_pointers_made==b.three_pointers_made && a.three_pointers_attempted==b.three_pointers_attempted &&
        a.free_throws_made==b.free_throws_made && a.free_throws_attempted==b.free_throws_attempted &&
        a.rebounds==b.rebounds && a.assists==b.assists && a.steals==b.steals && a.blocks==b.blocks &&
        a.turnovers==b.turnovers && a.fouls==b.fouls;
}
void tests(const std::filesystem::path& root) {
    const auto path=root/"legacy.n97sav";write(path,fixture());
    nba97::UserProfileStore store;check(store.load(path)==nba97::ProfileLoadStatus::Loaded,"v1 load");
    const auto bytes=read(path);const auto generation=store.generation();
    const auto original=store.profiles();
    check(store.atSlot(0)->name=="LEGACY USER" && store.atSlot(2)->id==102,"v1 import order");
    check(store.atSlot(0)->stats.games==0x80000000u && store.atSlot(2)->stats.fouls==0x800000d7u,"keyed legacy stats");
    check(store.acceptExact(0,100,"LEGACY USER",false) && store.rename(0,"LEGACY USER"),"no-op acceptance");
    check(read(path)==bytes && store.generation()==generation && !std::filesystem::exists(path.string()+".lock"),"read/no-op never writes");
    check(store.acceptExact(0,100,"Legacy User",false),"exact case rename");
    check(store.atSlot(0)->id==100 && store.atSlot(0)->created_unix_seconds==1000 &&
          sameStats(store.atSlot(0)->stats,original[0].stats),"rename preserves identity and all stats");
    check(store.acceptExact(1,101,"LEGACY USER",false),"case-sensitive names remain distinct");
    check(!store.acceptExact(2,102,"Legacy User",false),"exact duplicate refused");
    check(store.eraseExact(1,101),"delete fixed slot");
    nba97::UserProfileStore reopened;reopened.load(path);
    check(!reopened.atSlot(1) && reopened.atSlot(2)->id==102,"fixed slot deletion hole survives reopen");
    check(reopened.acceptExact(1,0,"a?!_'",true),"exact original punctuation");
    check(reopened.atSlot(1)->id && reopened.atSlot(1)->stats.games==0 && !reopened.atSlot(1)->controls_valid,"new profile reset");
    const auto snapshot=reopened.profiles();const auto saved=read(path);const auto gen=reopened.generation();
    std::filesystem::create_directory(path.string()+".tmp");
    bool failed=false;try {reopened.acceptExact(2,102,"FAIL",false);} catch(const std::exception&) {failed=true;}
    check(failed && reopened.atSlot(2)->name=="THIRD" && reopened.generation()==gen && read(path)==saved,"failed write rollback");
    check(reopened.profiles().size()==snapshot.size(),"failed write retains slots");

    const auto controls_path=root/"controls.n97sav";write(controls_path,fixture(2));
    nba97::UserProfileStore controls;controls.load(controls_path);const auto before=*controls.atSlot(7);
    check(before.controls_valid==3 && before.controls[58]==118,"raw controls/validity parsed");
    check(controls.acceptExact(7,101,"lower",false),"controlled profile rename");
    nba97::UserProfileStore controls2;controls2.load(controls_path);
    check(controls2.atSlot(7)->controls==before.controls && controls2.atSlot(7)->controls_valid==3 &&
          sameStats(controls2.atSlot(7)->stats,before.stats),"all controls/stats retained on rename/reopen");
    check(controls.acceptExact(19,102,"FIRST",false),"first writer");
    const auto current=read(controls_path);failed=false;
    try {controls2.acceptExact(19,102,"STALE",false);} catch(const std::exception&) {failed=true;}
    check(failed && controls2.atSlot(19)->name=="THIRD" && read(controls_path)==current,"stale writer refusal");
    auto future_backup=fixture(2);put(future_backup,8,3,2);crc(future_backup);
    const auto protected_path=root/"future-backup.n97sav";write(protected_path,fixture(2));
    write(protected_path.string()+".bak",future_backup);
    nba97::UserProfileStore protected_store;protected_store.load(protected_path);failed=false;
    try {protected_store.acceptExact(7,101,"changed",false);} catch(const std::exception&) {failed=true;}
    check(failed && read(protected_path)==fixture(2) && read(protected_path.string()+".bak")==future_backup &&
          protected_store.atSlot(7)->name=="SECOND","unsupported backup must not be overwritten");
    auto newer=fixture(2);put(newer,12,42,8);put(newer,240,99999999);crc(newer);
    const auto newer_path=root/"newer-backup.n97sav";write(newer_path,fixture(2));write(newer_path.string()+".bak",newer);
    nba97::UserProfileStore older;older.load(newer_path);failed=false;
    try {older.acceptExact(7,101,"changed",false);} catch(const std::exception&) {failed=true;}
    check(failed && read(newer_path)==fixture(2) && read(newer_path.string()+".bak")==newer,"newer backup must not be overwritten");
    for(bool missing:{false,true}) {
        const auto recovered_path=root/(missing ? "missing.n97sav":"corrupt.n97sav");
        if(!missing)write(recovered_path,Bytes{'b','a','d'});
        write(recovered_path.string()+".bak",fixture(2));
        nba97::UserProfileStore recovered;
        check(recovered.load(recovered_path)==nba97::ProfileLoadStatus::RecoveredBackup,"backup recovery");
        check(recovered.atSlot(7)->id==101 && recovered.acceptExact(7,101,"recovered",false),"recovered transaction");
        check(read(recovered_path.string()+".bak")==fixture(2),"known-good backup lost during repair");
        nba97::UserProfileStore repaired;repaired.load(recovered_path);
        check(repaired.atSlot(7)->name=="recovered" && repaired.atSlot(7)->controls_valid==3,"repaired primary");
    }

    for(unsigned which=0;which<7;++which) {
        auto b=fixture(2);const unsigned prof=88,stat=232,ctrl=448;
        if(which==0) put(b,8,3,2);
        if(which==1) put(b,10,1,2);
        if(which==2) put(b,prof+48,100,8);
        if(which==3) put(b,stat+72,102,8);
        if(which==4) b[ctrl+72+8]=b[ctrl+8];
        if(which==5) put(b,44,40);
        if(which==6) std::copy_n("MORE",4,b.begin()+72);
        crc(b);const auto bad=root/("invalid-"+std::to_string(which)+".n97sav");write(bad,b);
        if(which==0 || which==1 || which==6)write(bad.string()+".bak",fixture());
        nba97::UserProfileStore invalid;bool rejected=false;
        try {invalid.load(bad);} catch(const std::exception&) {rejected=true;}
        check(rejected,"invalid/newer format accepted or downgraded");
        rejected=false;try {invalid.save();} catch(const std::exception&) {rejected=true;}
        check(rejected && read(bad)==b,"failed load allowed save");
    }
    auto max=fixture();put(max,12,(std::numeric_limits<uint64_t>::max)(),8);crc(max);
    const auto exhausted=root/"exhausted.n97sav";write(exhausted,max);
    nba97::UserProfileStore full;full.load(exhausted);failed=false;
    try {full.acceptExact(0,100,"changed",false);} catch(const std::exception&) {failed=true;}
    check(failed && full.atSlot(0)->name=="LEGACY USER" && read(exhausted)==max,"generation exhaustion rollback");
}
}
int main() {try {
    const auto nonce=std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto root=std::filesystem::path(NBA97_SOURCE_DIR)/".local/verification/team_select"/("profile-unit-"+std::to_string(nonce));
    std::filesystem::create_directories(root);tests(root);std::cout<<"PROFILE V2 PASS: "<<root<<'\n';return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
