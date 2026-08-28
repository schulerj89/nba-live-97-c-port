#include "roster_database.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <cstdint>
#include <iostream>
#include <new>
#include <sstream>
#include <iomanip>
#include <cstdio>
#ifdef _MSC_VER
#include <crtdbg.h>
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib,"dbghelp.lib")
#endif

// Test-only allocator fault injection. This executable is single-threaded;
// production allocation and the normal test executables are unchanged.
namespace faults { std::size_t remaining=SIZE_MAX; unsigned site=0; const char* phase="startup"; bool fired=false; }
void* operator new(std::size_t n) {
    if(faults::remaining!=SIZE_MAX) {
        // Fail this allocation, not allocations used by MSVC's exception/
        // iterator-debug cleanup. Each allocation site is swept independently.
        if(!faults::remaining) {faults::remaining=SIZE_MAX; faults::fired=true; throw std::bad_alloc();}
        --faults::remaining;
    }
    if(auto* p=std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}

namespace {
using namespace nba97;
using Bytes=std::vector<std::uint8_t>;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
void pass(const char* s) {std::cout<<"SAVE-DB PASS "<<s<<'\n';}
std::string hex(const Sha256::Digest& d) {
    std::ostringstream s;
    for(auto b:d) s<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);
    return s.str();
}
void shaTests() {
    const std::pair<std::string,const char*> vectors[]={
        {"","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq","248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
        {std::string(1000000,'a'),"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"}};
    for(const auto& v:vectors) for(std::size_t chunk:{1,7,55,56,63,64,65,1024}) {
        Sha256 hash;
        for(std::size_t at=0;at<v.first.size();at+=chunk)
            hash.update(v.first.data()+at,(std::min)(chunk,v.first.size()-at));
        check(hex(hash.digest())==v.second && hex(hash.digest())==v.second,"SHA256 streaming/golden vector");
    }
    Sha256 hash; hash.update("ab",2); const auto prefix=hash.digest(); hash.update(nullptr,0); hash.update("c",1);
    check(prefix!=hash.digest() && hex(hash.digest())==vectors[1].second,"digest mutated stream");
    bool rejected=false; try {hash.update(nullptr,1);} catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"null hash data accepted"); pass("sha256_known_answers_chunk_boundaries_and_nondestructive_digest");
}

// Synthetic 60-player v4/v5 pack. Original assets/names are never embedded.
Bytes pack(unsigned version=5,bool reversed=false,unsigned padding=0) {
    const unsigned count=version==5 ? 5 : 4, start=24+20*count+padding;
    const unsigned play=start, team=play+60*127, strings=team+29*74, fall=strings+15, free=fall+50;
    Bytes b(free+(version==5 ? 200 : 0),0);
    auto half=[&](unsigned at,unsigned v) {b[at]=static_cast<std::uint8_t>(v); b[at+1]=static_cast<std::uint8_t>(v>>8);};
    auto word=[&](unsigned at,unsigned v) {half(at,v); half(at+2,v>>16);};
    std::copy_n("N97RDB\0\0",8,b.begin()); word(8,version); word(12,0x12345678); word(16,count); word(20,static_cast<unsigned>(b.size()));
    unsigned directory=24;
    auto section=[&](const char* tag,unsigned offset,unsigned n,unsigned stride) {
        std::copy_n(tag,4,b.begin()+directory); word(directory+4,offset); word(directory+8,n*stride);
        word(directory+12,n); word(directory+16,stride); directory+=20;
    };
    section("PLAY",play,60,127); section("TEAM",team,29,74); section("STRS",strings,15,1); section("FALL",fall,25,2);
    if(version==5) section("FREE",free,100,2);
    std::copy_n("unused\0Fixture\0",15,b.begin()+strings);
    for(unsigned i=0;i<60;++i) {
        const unsigned at=play+(reversed ? 59-i : i)*127;
        half(at,1000+i); b[at+7]=static_cast<std::uint8_t>(i%99); b[at+8]=static_cast<std::uint8_t>(i%5);
        std::fill_n(b.begin()+at+14,17,50);
        for(unsigned j=0;j<7;++j) word(at+99+j*4,7);
    }
    for(unsigned i=0;i<29;++i) {
        const unsigned at=team+(reversed ? 28-i : i)*74;
        half(at,i); half(at+2,2);
        for(unsigned j=0;j<5;++j) word(at+4+j*4,7);
        for(unsigned j=0;j<15;++j) half(at+24+j*2,j<2 ? 1000+i*2+j : 0xffff);
    }
    for(unsigned i=0;i<25;++i) half(fall+i*2,1000+i);
    if(version==5) for(unsigned i=0;i<100;++i) half(free+i*2,i<2 ? 1058+i : 0xffff);
    return b;
}
struct Fixture {
    std::filesystem::path dir,path;
    Fixture() {
        const auto seed=std::chrono::steady_clock::now().time_since_epoch().count();
        for(int i=0;i<100;++i) {
            auto p=std::filesystem::temp_directory_path()/("nba97-save-db-"+std::to_string(seed)+"-"+std::to_string(i));
            if(std::filesystem::create_directory(p)) {dir=p; break;}
        }
        check(!dir.empty(),"unique fixture directory"); path=dir/"synthetic.n97db";
    }
    void write(const Bytes& b) {
        std::ofstream out(path,std::ios::binary|std::ios::trunc);
        out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
        check(bool(out),"fixture write");
    }
    ~Fixture() {std::error_code e; std::filesystem::remove(path,e); std::filesystem::remove(dir,e);}
};
void databaseTests(const std::filesystem::path& path,const char* label) {
    RosterDatabase live; live.load(path);
    const auto original=live.slotTable(); const auto identity=live.baseIdentity();
    const auto* player=live.player(original[0]);
    const auto name=live.team(0)->city;
    check(live.originalSlots()==original && !live.differsFromOriginal(),"original baseline");
    auto changed=original; std::swap(changed[0],changed[1]);
    auto prepared=live.prepareSlotTable(changed);
    check(prepared.differsFromOriginal() && prepared.originalSlots()==original && prepared.baseIdentity()==identity &&
          &prepared.players()==&live.players() && &prepared.originalSlots()==&live.originalSlots() &&
          prepared.team(0)->city.data()==name.data() && live.slotTable()==original,"prepared candidate changed base/live");
    RosterSaveDocument doc; doc.generation=1; doc.slots=prepared.slotTable();
    const auto encoded=encodeRosterSave(doc,live.originalSlots(),live.baseIdentity());
    const auto decoded=decodeRosterSave(encoded,live.originalSlots(),live.baseIdentity());
    auto loaded=live.prepareSlotTable(decoded.slots);
    // Deliberately disable ALL allocations at the publication point.
    faults::remaining=0; live.swap(loaded); const bool publish_no_alloc=faults::remaining==0; faults::remaining=SIZE_MAX;
    check(publish_no_alloc,"publication allocated");
    check(live.slotTable()==changed && loaded.slotTable()==original && live.player(original[0])==player &&
          live.baseIdentity()==identity && live.differsFromOriginal(),"no-fail publication/derived pointers");
    const auto resolved=live.resolveTeamSlots(0);
    check(resolved[0]->id==changed[0] && resolved[1]->id==changed[1] && live.rosterOwner(changed[0])==0,
          "published derived caches stale");
    auto reset=live.prepareSlotTable(live.originalSlots());
    faults::remaining=0; live.swap(reset); const bool reset_no_alloc=faults::remaining==0; faults::remaining=SIZE_MAX;
    check(reset_no_alloc,"reset publication allocated");
    check(!live.differsFromOriginal() && live.slotTable()==original && reset.slotTable()==changed,
          "prepared reset baseline or retained prior state");
    for(unsigned failure=0;failure<4;++failure) {
        auto bad=original;
        if(failure==0) bad[0]=bad[1];
        if(failure==1) bad[0]=0x7fff;
        if(failure==2) std::swap(bad[0],bad[14]);
        if(failure==3) std::swap(bad[435],bad[534]);
        bool refused=false;
        try { (void)live.prepareSlotTable(bad); } catch(const std::runtime_error&) {refused=true;}
        check(refused && live.slotTable()==original && live.baseIdentity()==identity,"bad candidate published");
    }
    auto transfer=original; std::swap(transfer[0],transfer[435]);
    const auto traded=live.prepareSlotTable(transfer);
    check(traded.rosterOwner(transfer[0])==0 && traded.rosterOwner(transfer[435])==29 &&
          live.rosterOwner(transfer[0])==29,"prepared membership indexes not rebuilt");
    std::cout<<"SAVE-DB PASS "<<label<<"_codec_prepare_publish_reset_shared_base; identity="<<hex(identity)
             <<" one_team_bytes="<<encoded.size()<<" disk_writes=none\n";
}
void canonicalAndFailureTests() {
    Fixture fixture; const auto initial=pack(); fixture.write(initial);
    RosterDatabase live; live.load(fixture.path); const auto identity=live.baseIdentity(); const auto original=live.originalSlots();
    // Independently serialized with Python struct.pack/hashlib; catches a
    // stable but incorrect field order as well as accidental schema drift.
    check(hex(identity)=="99dedda8b2cb8658a724dcf8db0c8316cca1837598dd66069ca01e6b820fa3e4",
          "canonical logical-schema golden changed");
    const auto* player=live.player(original[0]);
    for(auto variant : {pack(5,true,31),pack(4,false,17),pack(5,false,0)}) {
        fixture.write(variant); RosterDatabase repacked; repacked.load(fixture.path);
        check(repacked.baseIdentity()==identity,"packing version/order/padding changed logical identity");
    }
    Fixture relocated; relocated.write(initial); RosterDatabase elsewhere; elsewhere.load(relocated.path);
    check(elsewhere.baseIdentity()==identity,"path changed identity");
    // Valid logical changes: player rating, team metadata, original order,
    // fallback identity and string bytes must each invalidate the old base.
    const unsigned play=124,team=play+60*127,strings=team+29*74,fall=strings+15;
    for(unsigned variant=0;variant<5;++variant) {
        auto b=initial;
        if(variant==0) ++b[play+14];
        if(variant==1) ++b[team+54];
        if(variant==2) {std::swap(b[team+24],b[team+26]);std::swap(b[team+25],b[team+27]);}
        if(variant==3) ++b[fall];
        if(variant==4) b[strings+7]='M';
        fixture.write(b); RosterDatabase changed; changed.load(fixture.path);
        check(changed.baseIdentity()!=identity,"logical change kept identity");
        RosterSaveDocument doc; doc.generation=1; doc.slots=original;
        auto encoded=encodeRosterSave(doc,original,identity); bool refused=false;
        try {decodeRosterSave(encoded,changed.originalSlots(),changed.baseIdentity());}
        catch(const RosterSaveError& e) {refused=e.kind()==RosterSaveErrorKind::WrongBase;}
        check(refused,"wrong base not refused");
    }
    pass("canonical_identity_repacking_paths_versions_and_logical_changes");
    const auto assertUnchanged=[&] {
        check(live.baseIdentity()==identity && live.originalSlots()==original && live.slotTable()==original &&
              live.player(original[0])==player,"failure changed live database/base/pointers");
    };
    for(unsigned variant=0;variant<5;++variant) {
        auto b=initial;
        if(variant==0) b.resize(9);
        if(variant==1) b[team]=29;
        if(variant==2) b[play+1]=0x80;
        if(variant==3) b[play+127]=b[play]; // duplicate player ID
        if(variant==4) b[44+12]=28; // directory team count wrong
        fixture.write(b); bool refused=false;
        try {live.load(fixture.path);} catch(const std::runtime_error&) {refused=true;}
        check(refused,"malformed base accepted"); assertUnchanged();
    }
    pass("failed_base_reload_is_atomic");
    fixture.write(initial);
    unsigned prepare_failures=0,load_failures=0,tolerated=0;
    auto proposed=original; std::swap(proposed[0],proposed[1]);
    for(unsigned i=0;i<4096;++i) {
        bool ok=false; faults::phase="prepare"; faults::site=i; faults::fired=false; faults::remaining=i;
        try {(void)live.prepareSlotTable(proposed); ok=true;} catch(const std::bad_alloc&) {++prepare_failures;}
        faults::remaining=SIZE_MAX; assertUnchanged();
        if(ok && !faults::fired) break;
        if(ok) ++tolerated;
        check(i!=4095,"candidate allocation sweep did not reach success");
    }
    for(unsigned i=0;i<4096;++i) {
        bool ok=false; faults::phase="load"; faults::site=i; faults::fired=false; faults::remaining=i;
        try {live.load(fixture.path); ok=true;} catch(const std::exception&) {++load_failures;}
        faults::remaining=SIZE_MAX;
        if(ok) {
            check(live.baseIdentity()==identity && live.slotTable()==original,"successful reload changed logical base");
            if(!faults::fired) break;
            ++tolerated; player=live.player(original[0]);
        } else assertUnchanged();
        check(i!=4095,"load allocation sweep did not reach success");
    }
    check(prepare_failures>29 && load_failures>100,"allocation sweeps too small");
    std::cout<<"SAVE-DB PASS exhaustive_allocation_failure_points prepare="<<prepare_failures<<" load="<<load_failures
             <<" tolerated_failures="<<tolerated<<" publication_allocations=0\n";
    databaseTests(fixture.path,"synthetic");
}
}
int main(int argc,char** argv) {
    try {
#ifdef _MSC_VER
        // This CLI test must never open assertion/JIT dialogs on the desktop.
        // Keep failures visible on stderr and nonzero exit, not interactive UI.
        _set_error_mode(_OUT_TO_STDERR);
        _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
        for(int mode : {_CRT_WARN,_CRT_ERROR,_CRT_ASSERT}) {
            _CrtSetReportMode(mode,_CRTDBG_MODE_FILE);
            _CrtSetReportFile(mode,_CRTDBG_FILE_STDERR);
        }
#endif
        std::set_terminate([] {
            faults::remaining=SIZE_MAX;
            std::fprintf(stderr,"SAVE-DB TERMINATE phase=%s allocation=%u\n",faults::phase,faults::site);
#ifdef _MSC_VER
            auto process=GetCurrentProcess(); SymInitialize(process,nullptr,TRUE);
            void* frames[24]{}; const auto count=CaptureStackBackTrace(0,24,frames,nullptr);
            for(unsigned i=0;i<count;++i) {
                alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
                auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage);
                symbol->SizeOfStruct=sizeof(SYMBOL_INFO); symbol->MaxNameLen=MAX_SYM_NAME;
                DWORD64 displacement=0;
                if(SymFromAddr(process,reinterpret_cast<DWORD64>(frames[i]),&displacement,symbol))
                    std::fprintf(stderr,"  %s + %llu\n",symbol->Name,static_cast<unsigned long long>(displacement));
            }
#endif
            std::abort();
        });
        std::cout<<std::unitbuf;
        check(argc<=2,"usage: nba97_roster_database_save_tests [private roster.n97db]");
        shaTests(); canonicalAndFailureTests();
        if(argc==2) databaseTests(argv[1],"private");
        std::cout<<"SAVE-DB SUMMARY canonical identity/immutable defaults/prepared state tested; durable save and UI Reset covered by separate suites\n";
        return 0;
    } catch(const std::exception& e) {faults::remaining=SIZE_MAX; std::cerr<<"SAVE-DB FAIL "<<e.what()<<'\n'; return 1;}
}
