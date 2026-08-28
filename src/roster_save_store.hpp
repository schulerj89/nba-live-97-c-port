#pragma once
#include "roster_database.hpp"

namespace nba97 {
enum class RosterStoreErrorKind { Io, Conflict, Busy, AmbiguousCommit };
class RosterStoreError : public std::runtime_error {
public:
    RosterStoreError(RosterStoreErrorKind kind,const std::string& what):std::runtime_error(what),kind_(kind) {}
    RosterStoreErrorKind kind() const noexcept {return kind_;}
private: RosterStoreErrorKind kind_;
};
enum class RosterLoadOrigin { Defaults, Primary, RecoveredMissing, RecoveredInvalid };
enum class RosterSaveStage {
    Locked, TempCreated, PartialWrite, BeforeFlush, Flushed, TempVerified,
    BeforeBackupReplace, BackupReplaced, BeforePrimaryReplace, PrimaryReplaced, BeforeDirectorySync
};
struct RosterSaveHooks {
    void (*call)(RosterSaveStage,void*)=nullptr;
    void* context=nullptr;
};
struct RosterCommitResult {
    bool changed=false;
    // false after a successful replacement but failed post-commit sync/check.
    // The new state is published either way; never retry it as an old draft.
    bool sync_completed=true;
    std::uint64_t generation=0;
    std::size_t bytes=0;
};

class RosterSaveStore final {
public:
    explicit RosterSaveStore(std::filesystem::path path);
    RosterSaveStore(const RosterSaveStore&)=delete;
    RosterSaveStore& operator=(const RosterSaveStore&)=delete;
    RosterLoadOrigin load(RosterDatabase& live);
    RosterCommitResult commit(RosterDatabase& live,const RosterSlots& proposed,RosterSaveHooks hooks={});
    [[nodiscard]] const RosterSaveDocument& accepted() const noexcept {return accepted_;}
    [[nodiscard]] const std::filesystem::path& path() const noexcept {return path_;}
    [[nodiscard]] bool needsRepair() const noexcept {return repair_;}
    [[nodiscard]] bool requiresReload() const noexcept {return uncertain_;}
private:
    struct Fingerprint {
        bool exists=false;
        std::uint64_t size=0;
        RosterBaseIdentity digest{};
        bool operator==(const Fingerprint& o) const noexcept {return exists==o.exists && size==o.size && digest==o.digest;}
        bool operator!=(const Fingerprint& o) const noexcept {return !(*this==o);}
    };
    struct FileImage {Fingerprint fingerprint; std::vector<std::uint8_t> bytes;};
    static FileImage read(const std::filesystem::path&);
    static Fingerprint fingerprint(const std::vector<std::uint8_t>&);
    static RosterSaveDocument parse(const FileImage&,const RosterDatabase&);
    void protectAssets(const RosterDatabase&) const;
    std::filesystem::path path_,backup_,lock_;
    RosterSaveDocument accepted_;
    RosterBaseIdentity base_{};
    Fingerprint primary_stamp_,backup_stamp_;
    bool loaded_=false,repair_=false,uncertain_=false;
};
}
