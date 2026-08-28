#include "roster_save_store.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
using namespace nba97;
using Bytes=std::vector<std::uint8_t>;
using Path=std::filesystem::path;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
void pass(const char* why) {std::cout<<"SAVE-STORE PASS "<<why<<'\n';}
void write(const Path& p,const Bytes& b) {
    std::ofstream out(p,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size())); check(bool(out),"fixture write");
}
Bytes read(const Path& p) {std::ifstream in(p,std::ios::binary); check(bool(in),"fixture read"); return Bytes(std::istreambuf_iterator<char>(in),{});}
void patch(Bytes& b,unsigned at,unsigned v,unsigned n=4) {for(unsigned i=0;i<n;++i) {b[at+i]=static_cast<std::uint8_t>(v);v>>=8;}}
void seal(Bytes& b) {patch(b,static_cast<unsigned>(b.size()-4),rosterSaveCrc32(b.data(),b.size()-4));}
struct Fixture {
    Path dir,asset,save,backup;
    Fixture() {
        const auto seed=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<100;++i) {
            auto p=std::filesystem::temp_directory_path()/("nba97-store-test-"+std::to_string(seed)+"-"+std::to_string(i));
            if(std::filesystem::create_directory(p)) {dir=p;break;}
        }
        check(!dir.empty(),"unique fixture directory"); asset=dir/"synthetic.n97db"; save=dir/"default.n97rst"; backup=dir/"default.n97rst.bak";
        constexpr unsigned play=84,team=play+60*61,strings=team+29*74;
        Bytes b(strings+8,0); std::copy_n("N97RDB\0\0",8,b.begin());
        patch(b,8,1);patch(b,12,0x12345678);patch(b,16,3);patch(b,20,static_cast<unsigned>(b.size()));
        unsigned at=24;
        const auto section=[&](const char* tag,unsigned offset,unsigned n,unsigned stride) {
            std::copy_n(tag,4,b.begin()+at);patch(b,at+4,offset);patch(b,at+8,n*stride);patch(b,at+12,n);patch(b,at+16,stride);at+=20;
        };
        section("PLAY",play,60,61);section("TEAM",team,29,74);section("STRS",strings,8,1);
        std::copy_n("Fixture\0",8,b.begin()+strings);
        for(unsigned i=0;i<60;++i) patch(b,play+i*61,1000+i,2);
        for(unsigned i=0;i<29;++i) {
            const auto t=team+i*74;patch(b,t,i,2);patch(b,t+2,2,2);
            for(unsigned j=0;j<15;++j) patch(b,t+24+j*2,j<2 ? 1000+i*2+j : 65535,2);
        }
        write(asset,b);
    }
    ~Fixture() {
        // Only this uniquely created directory's immediate fixture files.
        std::error_code e;
        for(std::filesystem::directory_iterator it(dir,e),end;!e && it!=end;it.increment(e))
            std::filesystem::remove(it->path(),e);
        std::filesystem::remove(dir,e);
    }
    void noTemps() {
        for(const auto& f:std::filesystem::directory_iterator(dir))
            check(f.path().filename().string().find(".tmp-")==std::string::npos,"temporary file leaked");
    }
};
template<class F> void storeReject(F fn,RosterStoreErrorKind kind) {
    bool rejected=false;try {fn();}catch(const RosterStoreError& e) {rejected=e.kind()==kind;}
    check(rejected,"wrong/missing store rejection");
}
template<class F> void formatReject(F fn,RosterSaveErrorKind kind) {
    bool rejected=false;try {fn();}catch(const RosterSaveError& e) {rejected=e.kind()==kind;}
    check(rejected,"wrong/missing format rejection");
}
RosterSlots changed(const RosterDatabase& db,unsigned team=0) {auto s=db.slotTable(); std::swap(s[team*15],s[team*15+1]);return s;}
void normal() {
    Fixture f; RosterDatabase db;db.load(f.asset); const auto asset=read(f.asset); const auto original=db.originalSlots();
    RosterSaveStore store(f.save);
    check(store.load(db)==RosterLoadOrigin::Defaults && store.accepted().generation==0,"default load");
    check(!store.commit(db,original).changed && !std::filesystem::exists(f.save),"default no-op wrote primary");
    auto order=changed(db);const auto first=store.commit(db,order);
    check(first.changed && first.sync_completed && first.generation==1 && first.bytes==148 && db.slotTable()==order &&
          !std::filesystem::exists(f.backup),"first commit");
    const auto first_bytes=read(f.save);
    RosterDatabase restarted;restarted.load(f.asset);RosterSaveStore reload(f.save);
    check(reload.load(restarted)==RosterLoadOrigin::Primary && restarted.slotTable()==order &&
          restarted.originalSlots()==original && restarted.differsFromOriginal(),"restart lost accepted/base distinction");
    order=changed(db,1);check(store.commit(db,order).generation==2 && read(f.backup)==first_bytes,"second commit backup");
    const auto second=read(f.save);const auto time=std::filesystem::last_write_time(f.save);
    check(!store.commit(db,order).changed && read(f.save)==second && std::filesystem::last_write_time(f.save)==time,"no-op rewrote generation/file");
    const auto reset=store.commit(db,original);
    check(reset.changed && reset.generation==3 && reset.bytes==68 && !db.differsFromOriginal() && read(f.backup)==second,"durable empty-override reset");
    check(reload.load(restarted)==RosterLoadOrigin::Primary && restarted.slotTable()==original && reload.accepted().generation==3,
          "clean reset resurrected older backup");
    check(read(f.asset)==asset,"immutable source changed");f.noTemps();pass("first_second_noop_restart_and_empty_override_reset");
}
void recovery() {
    for(unsigned missing=0;missing<2;++missing) {
        Fixture f;RosterDatabase db;db.load(f.asset);RosterSaveStore store(f.save);store.load(db);
        store.commit(db,changed(db));const auto first=db.slotTable();store.commit(db,changed(db,1));const auto backup=read(f.backup);
        if(missing) std::filesystem::remove(f.save);else write(f.save,Bytes{1,2,3});
        check(store.load(db)==(missing ? RosterLoadOrigin::RecoveredMissing : RosterLoadOrigin::RecoveredInvalid) &&
              db.slotTable()==first && store.needsRepair(),"backup recovery state");
        const auto repaired=store.commit(db,db.slotTable());
        check(repaired.changed && repaired.generation==2 && !store.needsRepair() && read(f.backup)==backup,
              "repair no-op skipped or corrupt primary overwrote backup");
        check(store.load(db)==RosterLoadOrigin::Primary && db.slotTable()==first,"repair restart");
    }
    Fixture f;RosterDatabase db;db.load(f.asset);RosterSaveStore store(f.save);store.load(db);store.commit(db,changed(db));store.commit(db,changed(db,1));
    auto huge=read(f.save);huge.resize(kRosterSaveMaxBytes+1,0);write(f.save,huge);
    check(store.load(db)==RosterLoadOrigin::RecoveredInvalid,"bounded oversized recovery");
    store.commit(db,db.slotTable());f.noTemps();pass("missing_corrupt_oversized_primary_recovery_preserves_good_backup");
}
void incompatible() {
    Fixture f;RosterDatabase db;db.load(f.asset);RosterSaveStore store(f.save);store.load(db);store.commit(db,changed(db));store.commit(db,changed(db,1));
    const auto healthy=read(f.save);const auto state=db.slotTable();
    auto future=healthy;patch(future,8,2,2);seal(future);write(f.save,future);
    formatReject([&]{store.load(db);},RosterSaveErrorKind::Unsupported);
    check(db.slotTable()==state && read(f.save)==future,"unsupported primary changed/fell back");
    auto wrong=healthy;wrong[32]^=1;seal(wrong);write(f.save,wrong);
    formatReject([&]{store.load(db);},RosterSaveErrorKind::WrongBase);
    write(f.save,healthy);write(f.backup,future);store.load(db);
    formatReject([&]{store.commit(db,changed(db));},RosterSaveErrorKind::Unsupported);
    check(read(f.save)==healthy && read(f.backup)==future && db.slotTable()==state,"incompatible backup overwritten");
    RosterSaveDocument next;next.slots=state;next.generation=UINT64_MAX;
    write(f.save,encodeRosterSave(next,db.originalSlots(),db.baseIdentity()));std::filesystem::remove(f.backup);store.load(db);
    storeReject([&]{store.commit(db,changed(db));},RosterStoreErrorKind::Conflict);
    check(!store.commit(db,state).changed,"maximum-generation no-op refused");
    pass("unsupported_wrong_base_protected_backup_and_generation_overflow");
}
void extensions() {
    Fixture f;RosterDatabase db;db.load(f.asset);RosterSaveDocument doc;doc.slots=db.originalSlots();doc.generation=7;doc.minor_version=3;
    doc.extensions={{{'F','U','T','R'},9,{0,1,255,0,3}}};write(f.save,encodeRosterSave(doc,db.originalSlots(),db.baseIdentity()));
    RosterSaveStore store(f.save);store.load(db);store.commit(db,changed(db));
    const auto saved=decodeRosterSave(read(f.save),db.originalSlots(),db.baseIdentity());
    check(saved.generation==8 && saved.minor_version==3 && saved.extensions.size()==1 &&
          saved.extensions[0].version==9 && saved.extensions[0].payload==doc.extensions[0].payload,"future optional data lost");
    pass("optional_extension_bytes_survive_actual_disk_commit");
}
void conflicts() {
    Fixture f;RosterDatabase a,b;a.load(f.asset);b.load(f.asset);RosterSaveStore one(f.save),two(f.save);one.load(a);two.load(b);
    one.commit(a,changed(a));storeReject([&]{two.commit(b,changed(b,1));},RosterStoreErrorKind::Conflict);
    check(b.slotTable()==b.originalSlots(),"stale writer changed memory");
    two.load(b);write(f.backup,read(f.save));storeReject([&]{two.commit(b,changed(b,1));},RosterStoreErrorKind::Conflict);
    struct Busy {RosterSaveStore* other;RosterDatabase* db;bool seen=false;};
    Busy context{&two,&b};one.load(a);
    RosterSaveHooks hooks{[](RosterSaveStage s,void* p) {
        if(s==RosterSaveStage::Locked) {auto& c=*static_cast<Busy*>(p);storeReject([&]{c.other->load(*c.db);},RosterStoreErrorKind::Busy);c.seen=true;}
    },&context};
    one.commit(a,changed(a,1),hooks);check(context.seen,"concurrent lock not exercised");
    pass("stale_writers_backup_edits_and_exclusive_lock");
}
struct Inject {RosterSaveStage target;bool fired=false;};
RosterSaveHooks inject(Inject& i) {return {[](RosterSaveStage s,void* p) {auto& i=*static_cast<Inject*>(p);if(s==i.target) {i.fired=true;throw std::runtime_error("injected file transaction failure");}},&i};}
void failures() {
    const RosterSaveStage before[]={RosterSaveStage::Locked,RosterSaveStage::TempCreated,RosterSaveStage::PartialWrite,
        RosterSaveStage::BeforeFlush,RosterSaveStage::Flushed,RosterSaveStage::TempVerified,
        RosterSaveStage::BeforeBackupReplace,RosterSaveStage::BackupReplaced,RosterSaveStage::BeforePrimaryReplace};
    unsigned count=0;
    for(unsigned existing=0;existing<2;++existing) for(auto where:before) {
        if(!existing && (where==RosterSaveStage::BeforeBackupReplace || where==RosterSaveStage::BackupReplaced)) continue;
        Fixture f;RosterDatabase db;db.load(f.asset);RosterSaveStore store(f.save);store.load(db);
        if(existing) store.commit(db,changed(db));
        const auto baseline=db.slotTable();const auto generation=store.accepted().generation;
        const auto previous=existing ? read(f.save) : Bytes{};const auto proposed=changed(db,1);
        Inject failure{where};bool caught=false;
        try {store.commit(db,proposed,inject(failure));}catch(const std::runtime_error&) {caught=true;}
        check(caught && failure.fired && db.slotTable()==baseline && store.accepted().generation==generation,"precommit failure published memory");
        check(existing ? read(f.save)==previous : !std::filesystem::exists(f.save),"precommit failure changed primary");
        if(std::filesystem::exists(f.backup)) check(decodeRosterSave(read(f.backup),db.originalSlots(),db.baseIdentity()).slots==baseline,"bad backup after failure");
        f.noTemps();const auto retry=store.commit(db,proposed);
        check(retry.generation==generation+1 && db.slotTable()==proposed,"retry failed or duplicated generation");++count;
    }
    for(auto where:{RosterSaveStage::PrimaryReplaced,RosterSaveStage::BeforeDirectorySync}) {
        Fixture f;RosterDatabase db;db.load(f.asset);RosterSaveStore store(f.save);store.load(db);const auto proposed=changed(db);Inject failure{where};
        const auto result=store.commit(db,proposed,inject(failure));
        check(failure.fired && result.changed && !result.sync_completed && result.generation==1 && db.slotTable()==proposed,
              "postcommit uncertainty retained old memory");
        check(!store.commit(db,proposed).changed && store.accepted().generation==1,"postcommit retry applied twice");
        check(store.load(db)==RosterLoadOrigin::Primary && db.slotTable()==proposed,"postcommit restart");f.noTemps();
    }
    std::cout<<"SAVE-STORE PASS failure_injection precommit="<<count<<" postcommit=2 retries_and_cleanup_verified\n";
}
void physicalFailure() {
#ifdef _WIN32
    Fixture f;RosterDatabase db;db.load(f.asset);RosterSaveStore store(f.save);store.load(db);store.commit(db,changed(db));
    const auto before=read(f.save);const auto state=db.slotTable();
    const auto handle=CreateFileW(f.save.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(handle!=INVALID_HANDLE_VALUE,"fixture sharing lock");
    bool caught=false;try {store.commit(db,changed(db,1));}catch(const RosterStoreError&) {caught=true;}
    CloseHandle(handle);
    check(caught && read(f.save)==before && db.slotTable()==state,"real OS sharing denial changed primary/state");
    store.commit(db,changed(db,1));f.noTemps();pass("windows_real_sharing_denial_then_retry");
#endif
}
void targetSafety() {
    for(unsigned target=0;target<3;++target) {
        Fixture f;RosterDatabase db;db.load(f.asset);const auto asset=read(f.asset);
        const auto alias=target==0 ? f.save : target==1 ? f.backup : Path(f.save.string()+".lock");
        std::filesystem::create_hard_link(f.asset,alias);
        RosterSaveStore store(f.save);
        storeReject([&]{store.load(db);},RosterStoreErrorKind::Io);
        check(read(f.asset)==asset && !db.differsFromOriginal(),"asset alias changed source");
    }
    Fixture f;RosterDatabase db;db.load(f.asset);std::filesystem::create_directory(f.save);
    RosterSaveStore store(f.save);storeReject([&]{store.load(db);},RosterStoreErrorKind::Io);
    std::filesystem::remove(f.save);
    pass("hardlink_asset_aliases_and_directory_targets_rejected");
}
void privateRoundTrip(const Path& asset) {
    Fixture f;const auto before=read(asset);RosterDatabase db;db.load(asset);RosterSaveStore store(f.save);
    store.load(db);const auto original=db.originalSlots();const auto proposed=changed(db);
    const auto saved=store.commit(db,proposed);
    RosterDatabase restarted;restarted.load(asset);RosterSaveStore reload(f.save);
    check(reload.load(restarted)==RosterLoadOrigin::Primary && restarted.slotTable()==proposed &&
          restarted.originalSlots()==original && saved.bytes==148,"private save/restart mismatch");
    check(reload.commit(restarted,original).bytes==68 && reload.load(restarted)==RosterLoadOrigin::Primary &&
          !restarted.differsFromOriginal() && read(asset)==before,"private reset/source immutability");
    f.noTemps();pass("private_catalogue_save_restart_reset_with_original_asset_unchanged");
}
}
int main(int argc,char** argv) {
    try {check(argc<=2,"usage: nba97_roster_save_store_tests [private roster.n97db]");
        normal();recovery();incompatible();extensions();conflicts();failures();physicalFailure();targetSafety();
        if(argc==2) privateRoundTrip(argv[1]);
        std::cout<<"SAVE-STORE SUMMARY temporary fixture files only; durable APIs exercised; power-loss hardware behavior not certified; UI covered separately by host tests\n";
        return 0;
    }catch(const std::exception& e) {std::cerr<<"SAVE-STORE FAIL "<<e.what()<<'\n';return 1;}
}
