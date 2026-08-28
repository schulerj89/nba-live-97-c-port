#include "roster_save_store.hpp"
#include "sha256.hpp"
#include <atomic>
#include <fstream>
#include <limits>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nba97 {
namespace {
using Path=std::filesystem::path;
using Bytes=std::vector<std::uint8_t>;
using Kind=RosterStoreErrorKind;
[[noreturn]] void fail(Kind kind,const char* message) {throw RosterStoreError(kind,message);}
void stage(RosterSaveHooks hooks,RosterSaveStage s) {if(hooks.call) hooks.call(s,hooks.context);}
void regularOrMissing(const Path& path,bool check_links=true) {
    std::error_code error;
    const auto status=std::filesystem::symlink_status(path,error);
    if(error==std::errc::no_such_file_or_directory) return;
    if(error) throw RosterStoreError(Kind::Io,"cannot inspect roster file: "+error.message());
    if(status.type()==std::filesystem::file_type::not_found) return;
    if(!std::filesystem::is_regular_file(status) || (check_links && std::filesystem::hard_link_count(path)!=1))
        fail(Kind::Io,"roster files must be unlinked regular files (no symlinks/reparse directories/hardlinks)");
}
class Lock final {
public:
    explicit Lock(const Path& path) {
        regularOrMissing(path,false);
#ifdef _WIN32
        handle_=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(handle_==INVALID_HANDLE_VALUE) {
            const auto error=GetLastError();
            fail(error==ERROR_SHARING_VIOLATION ? Kind::Busy : Kind::Io,"cannot acquire roster writer lock");
        }
        BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(handle_,&info) || info.nNumberOfLinks!=1 ||
           (info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))) {
            CloseHandle(handle_); handle_=INVALID_HANDLE_VALUE;
            fail(Kind::Io,"roster lock is not an independent regular file");
        }
#else
        handle_=::open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
        if(handle_<0) fail(Kind::Io,"cannot open roster writer lock");
        struct stat info{};
        if(::fstat(handle_,&info)!=0 || !S_ISREG(info.st_mode) || info.st_nlink!=1) {
            ::close(handle_); handle_=-1; fail(Kind::Io,"roster lock is not an independent regular file");
        }
        if(::flock(handle_,LOCK_EX|LOCK_NB)!=0) {
            const auto error=errno; ::close(handle_); handle_=-1;
            fail(error==EWOULDBLOCK || error==EAGAIN ? Kind::Busy : Kind::Io,"roster writer lock is busy");
        }
#endif
    }
    ~Lock() {
#ifdef _WIN32
        if(handle_!=INVALID_HANDLE_VALUE) CloseHandle(handle_);
#else
        if(handle_>=0) ::close(handle_);
#endif
    }
    Lock(const Lock&)=delete;
private:
#ifdef _WIN32
    HANDLE handle_=INVALID_HANDLE_VALUE;
#else
    int handle_=-1;
#endif
};
class Temp final {
public:
    explicit Temp(const Path& target) {
        static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
        const auto pid=GetCurrentProcessId();
#else
        const auto pid=::getpid();
#endif
        for(unsigned attempt=0;attempt<128;++attempt) {
            path_=target; path_+=".tmp-"+std::to_string(pid)+"-"+std::to_string(++sequence);
#ifdef _WIN32
            handle_=CreateFileW(path_.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(handle_!=INVALID_HANDLE_VALUE) {owned_=true; return;}
            if(GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS) break;
#else
            handle_=::open(path_.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
            if(handle_>=0) {owned_=true; return;}
            if(errno!=EEXIST) break;
#endif
        }
        fail(Kind::Io,"cannot create unique same-directory roster temporary file");
    }
    ~Temp() {close(); if(owned_) {std::error_code ignored; std::filesystem::remove(path_,ignored);}}
    Temp(const Temp&)=delete;
    const Path& path() const noexcept {return path_;}
    void release() noexcept {owned_=false;}
    void close() noexcept {
#ifdef _WIN32
        if(handle_!=INVALID_HANDLE_VALUE) {CloseHandle(handle_); handle_=INVALID_HANDLE_VALUE;}
#else
        if(handle_>=0) {::close(handle_); handle_=-1;}
#endif
    }
    void write(const std::uint8_t* p,std::size_t n) {
        while(n) {
#ifdef _WIN32
            DWORD written=0;
            if(!WriteFile(handle_,p,static_cast<DWORD>(n),&written,nullptr) || !written)
                fail(Kind::Io,"roster temporary write failed");
#else
            const auto written=::write(handle_,p,n);
            if(written<0 && errno==EINTR) continue;
            if(written<=0) fail(Kind::Io,"roster temporary write failed");
#endif
            p+=written; n-=static_cast<std::size_t>(written);
        }
    }
    void flush() {
#ifdef _WIN32
        if(!FlushFileBuffers(handle_)) fail(Kind::Io,"roster file flush failed");
#else
        if(::fsync(handle_)!=0) fail(Kind::Io,"roster file fsync failed");
#endif
    }
private:
    Path path_;
    bool owned_=false;
#ifdef _WIN32
    HANDLE handle_=INVALID_HANDLE_VALUE;
#else
    int handle_=-1;
#endif
};
void replace(const Path& source,const Path& target) {
#ifdef _WIN32
    // Same directory/volume, no COPY_ALLOWED. The old primary is never first
    // deleted or renamed away. Backup publication is a separate prior step.
    if(!MoveFileExW(source.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        fail(Kind::Io,"roster file replacement failed");
#else
    if(::rename(source.c_str(),target.c_str())!=0) fail(Kind::Io,"roster file rename failed");
#endif
}
void syncDirectory(const Path& directory) {
#ifndef _WIN32
    const int fd=::open(directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(fd<0) fail(Kind::Io,"cannot open roster directory for fsync");
    const int result=::fsync(fd); ::close(fd);
    if(result!=0) fail(Kind::Io,"roster directory fsync failed");
#else
    (void)directory; // File flush + same-volume write-through move requested.
#endif
}
void swapDocument(RosterSaveDocument& a,RosterSaveDocument& b) noexcept {
    using std::swap;
    swap(a.generation,b.generation); swap(a.minor_version,b.minor_version);
    a.slots.swap(b.slots); a.extensions.swap(b.extensions);
}
bool recoverable(const RosterSaveError& e) {
    return e.kind()==RosterSaveErrorKind::Corrupt || e.kind()==RosterSaveErrorKind::InvalidRoster;
}
}

RosterSaveStore::RosterSaveStore(Path path):path_(std::filesystem::absolute(path).lexically_normal()) {
    if(path_.filename().empty() || path_.extension()!=L".n97rst")
        fail(Kind::Io,"roster save requires an explicit .n97rst filename");
    backup_=path_; backup_+=".bak"; lock_=path_; lock_+=".lock";
}
void RosterSaveStore::protectAssets(const RosterDatabase& db) const {
    for(const auto* path:{&path_,&backup_}) {
        regularOrMissing(*path);
        std::error_code error;
        if(std::filesystem::equivalent(*path,db.sourcePath(),error))
            fail(Kind::Io,"roster save target aliases immutable database asset");
    }
}
RosterSaveStore::Fingerprint RosterSaveStore::fingerprint(const Bytes& bytes) {
    Sha256 hash; hash.update(bytes.data(),bytes.size());
    return {true,bytes.size(),hash.digest()};
}
RosterSaveStore::FileImage RosterSaveStore::read(const Path& path) {
    regularOrMissing(path);
    std::error_code error;
    if(!std::filesystem::exists(path,error)) {
        if(error) fail(Kind::Io,"cannot inspect roster save existence");
        return {};
    }
    const auto size=std::filesystem::file_size(path);
    std::ifstream input(path,std::ios::binary);
    if(!input) fail(Kind::Io,"cannot read roster save");
    FileImage out; out.fingerprint.exists=true; out.fingerprint.size=size;
    // Even corrupt oversized files are hashed in fixed-size chunks so recovery
    // can detect an intervening edit. Never allocate from their claimed size.
    const auto retained=static_cast<std::size_t>((std::min)(size,std::uintmax_t(kRosterSaveMaxBytes)));
    out.bytes.reserve(retained);
    Sha256 hash; std::array<std::uint8_t,4096> chunk{};
    std::uintmax_t left=size;
    while(left) {
        const auto n=static_cast<std::size_t>((std::min)(left,std::uintmax_t(chunk.size())));
        input.read(reinterpret_cast<char*>(chunk.data()),static_cast<std::streamsize>(n));
        if(static_cast<std::size_t>(input.gcount())!=n) fail(Kind::Conflict,"roster changed or became unreadable during read");
        hash.update(chunk.data(),n);
        const auto keep=(std::min)(n,retained-out.bytes.size());
        out.bytes.insert(out.bytes.end(),chunk.begin(),chunk.begin()+keep);
        left-=n;
    }
    char extra=0;
    if(input.get(extra)) fail(Kind::Conflict,"roster grew during read");
    if(!input.eof()) fail(Kind::Io,"roster read failed at EOF");
    out.fingerprint.digest=hash.digest(); return out;
}
RosterSaveDocument RosterSaveStore::parse(const FileImage& file,const RosterDatabase& db) {
    // Decode the retained prefix first: a future major version must remain
    // Unsupported even when it has a larger file-size limit than this version.
    auto doc=decodeRosterSave(file.bytes,db.originalSlots(),db.baseIdentity());
    if(file.fingerprint.size>kRosterSaveMaxBytes)
        throw RosterSaveError(RosterSaveErrorKind::Corrupt,"roster save exceeds size limit");
    try { (void)db.prepareSlotTable(doc.slots); }
    catch(const std::runtime_error&) {throw RosterSaveError(RosterSaveErrorKind::InvalidRoster,"roster save violates list invariants");}
    return doc;
}
RosterLoadOrigin RosterSaveStore::load(RosterDatabase& live) {
    (void)live.baseIdentity(); protectAssets(live);
    std::filesystem::create_directories(path_.parent_path());
    Lock guard(lock_);
    const auto primary=read(path_),backup=read(backup_);
    RosterSaveDocument next; next.slots=live.originalSlots();
    auto origin=RosterLoadOrigin::Defaults;
    bool use_backup=!primary.fingerprint.exists;
    if(primary.fingerprint.exists) {
        try {next=parse(primary,live); origin=RosterLoadOrigin::Primary;}
        catch(const RosterSaveError& e) {if(!recoverable(e) || !backup.fingerprint.exists) throw; use_backup=true;}
    }
    if(use_backup && backup.fingerprint.exists) {
        next=parse(backup,live);
        origin=primary.fingerprint.exists ? RosterLoadOrigin::RecoveredInvalid : RosterLoadOrigin::RecoveredMissing;
    }
    auto prepared=live.prepareSlotTable(next.slots);
    base_=live.baseIdentity(); primary_stamp_=primary.fingerprint; backup_stamp_=backup.fingerprint;
    repair_=origin==RosterLoadOrigin::RecoveredMissing || origin==RosterLoadOrigin::RecoveredInvalid;
    swapDocument(accepted_,next); live.swap(prepared); loaded_=true; uncertain_=false;
    return origin;
}
RosterCommitResult RosterSaveStore::commit(RosterDatabase& live,const RosterSlots& proposed,RosterSaveHooks hooks) {
    if(!loaded_ || uncertain_) fail(Kind::Conflict,"roster store must load/reload before saving");
    if(live.baseIdentity()!=base_ || live.slotTable()!=accepted_.slots)
        fail(Kind::Conflict,"roster accepted state/base changed since load");
    protectAssets(live);
    Lock guard(lock_); stage(hooks,RosterSaveStage::Locked);
    auto primary=read(path_),backup=read(backup_);
    if(primary.fingerprint!=primary_stamp_ || backup.fingerprint!=backup_stamp_)
        fail(Kind::Conflict,"roster save or backup changed since load; refusing overwrite");
    if(proposed==accepted_.slots && !repair_) return {false,true,accepted_.generation,static_cast<std::size_t>(primary_stamp_.size)};
    if(accepted_.generation==(std::numeric_limits<std::uint64_t>::max)())
        fail(Kind::Conflict,"roster generation exhausted; refusing wrap");
    auto prepared=live.prepareSlotTable(proposed);
    auto next=accepted_; next.slots=proposed; ++next.generation;
    const auto encoded=encodeRosterSave(next,live.originalSlots(),base_);
    const auto new_stamp=fingerprint(encoded);
    // Do not overwrite an incompatible/newer backup just because primary is
    // readable. Corrupt old backup can safely be replaced by validated primary.
    if(backup.fingerprint.exists && !repair_) {
        try {
            const auto old=parse(backup,live);
            if(old.generation>accepted_.generation) fail(Kind::Conflict,"backup is newer than accepted primary");
        } catch(const RosterSaveError& e) {if(!recoverable(e)) throw;}
    }
    Temp temporary(path_); stage(hooks,RosterSaveStage::TempCreated);
    const auto half=encoded.size()/2;
    temporary.write(encoded.data(),half); stage(hooks,RosterSaveStage::PartialWrite);
    temporary.write(encoded.data()+half,encoded.size()-half);
    stage(hooks,RosterSaveStage::BeforeFlush); temporary.flush(); stage(hooks,RosterSaveStage::Flushed);
    temporary.close();
    if(read(temporary.path()).fingerprint!=new_stamp) fail(Kind::Io,"roster temporary verification failed");
    stage(hooks,RosterSaveStage::TempVerified);
    if(primary.fingerprint.exists && !repair_) {
        (void)parse(primary,live);
        Temp old(backup_); old.write(primary.bytes.data(),primary.bytes.size()); old.flush(); old.close();
        if(read(old.path()).fingerprint!=primary.fingerprint) fail(Kind::Io,"roster backup verification failed");
        stage(hooks,RosterSaveStage::BeforeBackupReplace);
        if(read(path_).fingerprint!=primary_stamp_ || read(backup_).fingerprint!=backup_stamp_)
            fail(Kind::Conflict,"roster files changed during preparation");
        try {replace(old.path(),backup_);}
        catch(...) {
            // A failed OS call may still have advanced the backup. Keep the
            // primary/live state unchanged and make the next retry explicit.
            try {
                const auto observed=read(backup_);
                if(observed.fingerprint==primary.fingerprint) backup_stamp_=primary.fingerprint;
                else if(observed.fingerprint!=backup_stamp_) uncertain_=true;
            } catch(...) {uncertain_=true;}
            throw;
        }
        old.release(); backup_stamp_=primary.fingerprint;
        syncDirectory(path_.parent_path()); stage(hooks,RosterSaveStage::BackupReplaced);
    }
    stage(hooks,RosterSaveStage::BeforePrimaryReplace);
    if(read(path_).fingerprint!=primary_stamp_ || read(backup_).fingerprint!=backup_stamp_)
        fail(Kind::Conflict,"roster files changed before commit");
    bool sync_ok=true;
    try {replace(temporary.path(),path_);}
    catch(...) {
        // Never describe a possibly committed replacement as a safe retry.
        FileImage observed;
        try {observed=read(path_);} catch(...) {uncertain_=true; fail(Kind::AmbiguousCommit,"cannot establish roster replacement outcome; reload required");}
        if(observed.fingerprint==new_stamp) sync_ok=false;
        else if(observed.fingerprint==primary_stamp_) throw;
        else {uncertain_=true; fail(Kind::AmbiguousCommit,"roster replacement outcome changed unexpectedly; reload required");}
    }
    temporary.release();
    // No throwing work may prevent publication after the replacement point.
    try {
        stage(hooks,RosterSaveStage::PrimaryReplaced);
        stage(hooks,RosterSaveStage::BeforeDirectorySync); syncDirectory(path_.parent_path());
    } catch(...) {sync_ok=false;}
    swapDocument(accepted_,next); live.swap(prepared); primary_stamp_=new_stamp; repair_=false;
    return {true,sync_ok,accepted_.generation,encoded.size()};
}
}
