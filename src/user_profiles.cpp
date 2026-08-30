#include "user_profiles.hpp"

#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_set>

namespace nba97 {
namespace {
constexpr std::array<std::uint8_t, 8> kMagic{'N','9','7','P','R','O','F',0};
constexpr std::uint16_t kMajorVersion = 2;
constexpr std::uint16_t kMinorVersion = 0;
constexpr std::uint32_t kHeaderSize = 40;
constexpr std::uint32_t kDirectoryEntrySize = 16;
constexpr std::uint32_t kProfileRecordSize = 48;
constexpr std::uint32_t kStatsRecordSize = 8 + 16 * 4;
constexpr std::size_t kNoIndex = (std::numeric_limits<std::size_t>::max)();

std::vector<std::uint8_t> readProfileBytes(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) throw std::runtime_error("cannot open profile save");
    const auto n=input.tellg();
    if(n<0 || n>16384) throw std::runtime_error("profile save exceeds size bound");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(n));input.seekg(0);
    if(n && !input.read(reinterpret_cast<char*>(bytes.data()),n)) throw std::runtime_error("truncated profile save");
    return bytes;
}
bool exactName(std::string_view name) {
    return !name.empty() && name.size()<=13 &&
        std::all_of(name.begin(),name.end(),[](unsigned char c){return c>=32 && c<=126;});
}
class ProfileWriteLock {
public:
    explicit ProfileWriteLock(const std::filesystem::path& path) {
        const auto lock=std::filesystem::path(path.wstring()+L".lock");
#ifdef _WIN32
        handle_=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle_==INVALID_HANDLE_VALUE) throw std::runtime_error("profile save is locked by another writer");
#else
        handle_=::open(lock.c_str(),O_CREAT|O_RDWR,0600);
        if(handle_<0 || flock(handle_,LOCK_EX|LOCK_NB)!=0) {
            if(handle_>=0) ::close(handle_);
            throw std::runtime_error("profile save is locked by another writer");
        }
#endif
    }
    ~ProfileWriteLock() {
#ifdef _WIN32
        CloseHandle(handle_);
#else
        flock(handle_,LOCK_UN);::close(handle_);
#endif
    }
    ProfileWriteLock(const ProfileWriteLock&)=delete;
    ProfileWriteLock& operator=(const ProfileWriteLock&)=delete;
private:
#ifdef _WIN32
    HANDLE handle_=INVALID_HANDLE_VALUE;
#else
    int handle_=-1;
#endif
};

void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>(value >> shift));
}

void appendU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8)
        out.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint16_t readU16(const std::vector<std::uint8_t>& in, std::size_t at) {
    if (at + 2 > in.size()) throw std::runtime_error("truncated u16");
    return static_cast<std::uint16_t>(in[at] | (in[at + 1] << 8));
}

std::uint32_t readU32(const std::vector<std::uint8_t>& in, std::size_t at) {
    if (at + 4 > in.size()) throw std::runtime_error("truncated u32");
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) value |= std::uint32_t(in[at++]) << shift;
    return value;
}

std::uint64_t readU64(const std::vector<std::uint8_t>& in, std::size_t at) {
    if (at + 8 > in.size()) throw std::runtime_error("truncated u64");
    std::uint64_t value = 0;
    for (int shift = 0; shift < 64; shift += 8) value |= std::uint64_t(in[at++]) << shift;
    return value;
}

void patchU32(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) out[at++] = static_cast<std::uint8_t>(value >> shift);
}

std::uint32_t crc32(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t value = 0xffffffffu;
    for (std::uint8_t byte : bytes) {
        value ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1u)));
    }
    return ~value;
}

std::uint64_t unixNow() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::uint64_t newId(const std::vector<UserProfile>& existing) {
    std::random_device source;
    std::mt19937_64 generator((std::uint64_t(source()) << 32) ^ source() ^ unixNow());
    for (;;) {
        const std::uint64_t candidate = generator();
        if (candidate != 0 && std::none_of(existing.begin(), existing.end(),
            [candidate](const UserProfile& profile) { return profile.id == candidate; }))
            return candidate;
    }
}

void appendStats(std::vector<std::uint8_t>& out, const UserCareerStats& stats) {
    for (const std::uint32_t value : std::array<std::uint32_t, 16>{
        stats.games, stats.wins, stats.losses, stats.points,
        stats.field_goals_made, stats.field_goals_attempted,
        stats.three_pointers_made, stats.three_pointers_attempted,
        stats.free_throws_made, stats.free_throws_attempted,
        stats.rebounds, stats.assists, stats.steals, stats.blocks,
        stats.turnovers, stats.fouls}) appendU32(out, value);
}

UserCareerStats readStats(const std::vector<std::uint8_t>& in, std::size_t at) {
    std::array<std::uint32_t, 16> values{};
    for (auto& value : values) { value = readU32(in, at); at += 4; }
    UserCareerStats result;
    result.games = values[0]; result.wins = values[1]; result.losses = values[2]; result.points = values[3];
    result.field_goals_made = values[4]; result.field_goals_attempted = values[5];
    result.three_pointers_made = values[6]; result.three_pointers_attempted = values[7];
    result.free_throws_made = values[8]; result.free_throws_attempted = values[9];
    result.rebounds = values[10]; result.assists = values[11]; result.steals = values[12];
    result.blocks = values[13]; result.turnovers = values[14]; result.fouls = values[15];
    return result;
}
} // namespace

bool UserProfileStore::isAllowedNameCharacter(char value) noexcept {
    const unsigned char byte = static_cast<unsigned char>(value);
    return (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') ||
           byte == ' ' || byte == '-' || byte == '.';
}

std::string UserProfileStore::normalizeName(std::string_view name) {
    std::string result;
    result.reserve((std::min)(name.size(), kMaximumUserNameLength));
    bool previous_space = true;
    for (char value : name) {
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
        if (!isAllowedNameCharacter(upper)) continue;
        if (upper == ' ' && previous_space) continue;
        if (result.size() == kMaximumUserNameLength) break;
        result.push_back(upper);
        previous_space = upper == ' ';
    }
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

ProfileLoadStatus UserProfileStore::load(const std::filesystem::path& path) {
    loaded_=false;unsupported_=false;recovered_backup_=false;primary_bytes_.clear();
    path_ = std::filesystem::absolute(path).lexically_normal();
    profiles_.clear();
    generation_ = 0;
    last_error_.clear();
    const auto backup = std::filesystem::path(path_.wstring() + L".bak");
    backup_exists_=std::filesystem::exists(backup);backup_bytes_.clear();backup_protected_=false;
    uint64_t backup_generation=0;bool backup_valid=false;
    if(backup_exists_) {
        try {
            backup_bytes_=readProfileBytes(backup);
            UserProfileStore probe;
            backup_valid=probe.readFile(backup);backup_generation=probe.generation_;
            backup_protected_=probe.unsupported_;
        } catch(...) {backup_protected_=true;}
    }
    primary_exists_=std::filesystem::exists(path_);
    if (!primary_exists_) {
        if(backup_exists_) {
            if(backup_protected_ || !readFile(backup)) throw std::runtime_error("missing primary with unreadable/unsupported profile backup");
            loaded_=true;recovered_backup_=true;return ProfileLoadStatus::RecoveredBackup;
        }
        loaded_=true;return ProfileLoadStatus::NewStore;
    }
    primary_bytes_=readProfileBytes(path_);
    if (readFile(path_)) {
        if(backup_valid && backup_generation>generation_) backup_protected_=true;
        loaded_=true;return ProfileLoadStatus::Loaded;
    }
    if(unsupported_) throw std::runtime_error("profile save format refused without backup downgrade: "+last_error_);
    const std::string primary_error = last_error_;
    profiles_.clear();
    generation_ = 0;
    if (std::filesystem::exists(backup) && readFile(backup)) {
        last_error_ = primary_error;
        loaded_=true;recovered_backup_=true;
        return ProfileLoadStatus::RecoveredBackup;
    }
    throw std::runtime_error("profile save is invalid: " + primary_error +
                             (last_error_.empty() ? "" : "; backup: " + last_error_));
}

bool UserProfileStore::readFile(const std::filesystem::path& path) {
    try {
        auto bytes=readProfileBytes(path);
        if (bytes.size() < kHeaderSize || !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()))
            throw std::runtime_error("bad magic or truncated header");
        const auto major=readU16(bytes,8),minor=readU16(bytes,10);
        if ((major!=1 && major!=2) || minor!=0) {
            unsupported_=true;throw std::runtime_error("unsupported profile version");
        }
        const std::uint32_t file_size = readU32(bytes, 24);
        const std::uint32_t stored_crc = readU32(bytes, 28);
        if (file_size != bytes.size()) throw std::runtime_error("file-size field mismatch");
        patchU32(bytes, 28, 0);
        if (crc32(bytes) != stored_crc) throw std::runtime_error("CRC32 mismatch");
        const std::uint32_t section_count = readU32(bytes, 20);
        if (section_count > 64 || kHeaderSize + section_count * kDirectoryEntrySize > bytes.size())
            throw std::runtime_error("invalid section directory");
        const auto generation=readU64(bytes,12);
        std::vector<UserProfile> candidate;
        std::size_t prof_offset = 0, prof_size = 0, prof_count = 0;
        std::size_t stat_offset = 0, stat_size = 0, stat_count = 0;
        std::size_t ctrl_offset=0,ctrl_size=0,ctrl_count=0;
        std::vector<std::pair<std::size_t,std::size_t>> extents;
        std::unordered_set<std::string> tags;
        for (std::uint32_t section = 0; section < section_count; ++section) {
            const std::size_t at = kHeaderSize + section * kDirectoryEntrySize;
            const std::string tag(reinterpret_cast<const char*>(bytes.data() + at), 4);
            const std::size_t offset = readU32(bytes, at + 4);
            const std::size_t size = readU32(bytes, at + 8);
            const std::size_t count = readU32(bytes, at + 12);
            if (offset<kHeaderSize+section_count*kDirectoryEntrySize ||
                offset > bytes.size() || size > bytes.size() - offset)
                throw std::runtime_error("section outside file");
            if(!tags.insert(tag).second) throw std::runtime_error("duplicate profile section");
            for(const auto& prior:extents) if(size && prior.second &&
                offset<prior.first+prior.second && prior.first<offset+size)
                throw std::runtime_error("overlapping profile sections");
            extents.push_back({offset,size});
            if (tag == "PROF") { prof_offset = offset; prof_size = size; prof_count = count; }
            else if (tag == "STAT") { stat_offset = offset; stat_size = size; stat_count = count; }
            else if(tag=="CTRL" && major==2) {ctrl_offset=offset;ctrl_size=size;ctrl_count=count;}
            else {unsupported_=true;throw std::runtime_error("unknown profile section; cannot preserve it");}
        }
        if (!prof_offset || prof_count > kMaximumUserProfiles || prof_size != prof_count * kProfileRecordSize)
            throw std::runtime_error("invalid PROF section");
        candidate.reserve(prof_count);
        std::unordered_set<uint64_t> ids;
        std::unordered_set<std::string> names;
        for (std::size_t i = 0; i < prof_count; ++i) {
            const std::size_t at = prof_offset + i * kProfileRecordSize;
            UserProfile profile;
            profile.id = readU64(bytes, at);
            profile.created_unix_seconds = readU64(bytes, at + 8);
            profile.updated_unix_seconds = readU64(bytes, at + 16);
            const std::size_t name_length = bytes[at + 24];
            if (!profile.id || !ids.insert(profile.id).second || name_length == 0 || name_length > kMaximumUserNameLength)
                throw std::runtime_error("invalid profile record");
            profile.name.assign(reinterpret_cast<const char*>(bytes.data() + at + 25), name_length);
            if (!(major==1 ? normalizeName(profile.name)==profile.name:exactName(profile.name)) ||
                !names.insert(profile.name).second)
                throw std::runtime_error("invalid or duplicate profile name");
            profile.slot=static_cast<uint8_t>(i);
            candidate.push_back(std::move(profile));
        }
        if (stat_offset) {
            if (stat_count>20 || stat_size != stat_count * kStatsRecordSize)
                throw std::runtime_error("invalid STAT section");
            std::unordered_set<uint64_t> stat_ids;
            for (std::size_t i = 0; i < stat_count; ++i) {
                const std::size_t at = stat_offset + i * kStatsRecordSize;
                const std::uint64_t id = readU64(bytes, at);
                const auto found = std::find_if(candidate.begin(), candidate.end(),
                    [id](const UserProfile& profile) { return profile.id == id; });
                if(found==candidate.end() || !stat_ids.insert(id).second)
                    throw std::runtime_error("orphan or duplicate STAT ID");
                found->stats = readStats(bytes, at + 8);
            }
        }
        if(major==2) {
            if(!stat_offset || stat_count!=prof_count || !ctrl_offset || ctrl_count!=prof_count ||
               ctrl_size!=ctrl_count*72) throw std::runtime_error("incomplete v2 STAT/CTRL sections");
            std::unordered_set<uint64_t> ctrl_ids;std::array<bool,20> slots{};
            for(std::size_t i=0;i<ctrl_count;++i) {
                const auto at=ctrl_offset+i*72;const auto id=readU64(bytes,at);
                auto found=std::find_if(candidate.begin(),candidate.end(),[id](const UserProfile& p){return p.id==id;});
                const auto slot=bytes[at+8];
                if(found==candidate.end() || !ctrl_ids.insert(id).second || slot>=20 || slots[slot])
                    throw std::runtime_error("invalid CTRL identity/slot");
                slots[slot]=true;found->slot=slot;found->controls_valid=bytes[at+9];
                std::copy_n(bytes.begin()+static_cast<std::ptrdiff_t>(at+10),59,found->controls.begin());
                if(bytes[at+69] || bytes[at+70] || bytes[at+71])
                    throw std::runtime_error("nonzero CTRL reserved bytes");
            }
        }
        profiles_=std::move(candidate);generation_=generation;
        last_error_.clear();
        return true;
    } catch (const std::exception& error) {
        last_error_ = error.what();
        profiles_.clear();
        generation_ = 0;
        return false;
    }
}

bool UserProfileStore::nameExists(std::string_view normalized, std::size_t except_index) const noexcept {
    for (std::size_t i = 0; i < profiles_.size(); ++i)
        if (i != except_index && profiles_[i].name == normalized) return true;
    return false;
}

bool UserProfileStore::create(std::string_view name, std::size_t* created_index) {
    last_error_.clear();
    const std::string normalized = normalizeName(name);
    if (normalized.empty()) { last_error_ = "name is empty"; return false; }
    if (profiles_.size() >= kMaximumUserProfiles) { last_error_ = "20-profile limit reached"; return false; }
    if (nameExists(normalized, kNoIndex)) { last_error_ = "name already exists"; return false; }
    uint8_t slot=0;while(slot<20 && atSlot(slot)) ++slot;
    if(!acceptExact(slot,0,normalized,true)) return false;
    if (created_index) *created_index = profiles_.size() - 1;
    return true;
}

bool UserProfileStore::rename(std::size_t index, std::string_view name) {
    last_error_.clear();
    if (index >= profiles_.size()) { last_error_ = "profile index out of range"; return false; }
    if(name==profiles_[index].name) return true; // Preserve unchanged mixed-case/legacy names.
    const std::string normalized = normalizeName(name);
    if (normalized.empty()) { last_error_ = "name is empty"; return false; }
    if (nameExists(normalized, index)) { last_error_ = "name already exists"; return false; }
    return acceptExact(profiles_[index].slot,profiles_[index].id,normalized,false);
}

bool UserProfileStore::erase(std::size_t index) {
    last_error_.clear();
    if (index >= profiles_.size()) { last_error_ = "profile index out of range"; return false; }
    return eraseExact(profiles_[index].slot,profiles_[index].id);
}

const UserProfile* UserProfileStore::atSlot(uint8_t slot) const noexcept {
    for(const auto& p:profiles_) if(p.slot==slot) return &p;
    return nullptr;
}
bool UserProfileStore::acceptExact(uint8_t slot,uint64_t expected,std::string_view name,bool clear) {
    last_error_.clear();
    const auto* old=atSlot(slot);
    if(!loaded_ || slot>=20 || (old ? old->id:0)!=expected || !exactName(name)) {
        last_error_="invalid or stale profile transaction";return false;
    }
    const auto index=old ? static_cast<std::size_t>(old-profiles_.data()):kNoIndex;
    if(nameExists(name,index)) {last_error_="name already exists";return false;}
    if(old && !clear && old->name==name) return true;
    auto previous=profiles_;
    try {
        UserProfile next=(old && !clear) ? *old:UserProfile{};
        if(!next.id) {next.id=newId(profiles_);next.created_unix_seconds=unixNow();}
        next.updated_unix_seconds=unixNow();next.name=name;next.slot=slot;
        if(old) profiles_[index]=std::move(next);else profiles_.push_back(std::move(next));
        save();
    } catch(...) {profiles_.swap(previous);throw;}
    return true;
}
bool UserProfileStore::eraseExact(uint8_t slot,uint64_t expected) {
    last_error_.clear();
    const auto* old=atSlot(slot);
    if(!loaded_ || !old || !expected || old->id!=expected) {last_error_="invalid or stale profile deletion";return false;}
    const auto index=static_cast<std::size_t>(old-profiles_.data());auto previous=profiles_;
    try {profiles_.erase(profiles_.begin()+static_cast<std::ptrdiff_t>(index));save();}
    catch(...) {profiles_.swap(previous);throw;}
    return true;
}

std::vector<std::uint8_t> UserProfileStore::serialize(std::uint64_t next_generation) const {
    std::vector<std::uint8_t> out;
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    appendU16(out, kMajorVersion); appendU16(out, kMinorVersion);
    appendU64(out, next_generation); appendU32(out, 3); appendU32(out, 0);
    appendU32(out, 0); appendU32(out, 0); appendU32(out, 0); // file size, CRC, reserved
    const std::size_t directory = out.size();
    out.resize(out.size() + 3 * kDirectoryEntrySize, 0);
    const std::size_t prof_offset = out.size();
    for (const auto& profile : profiles_) {
        appendU64(out, profile.id); appendU64(out, profile.created_unix_seconds);
        appendU64(out, profile.updated_unix_seconds); out.push_back(static_cast<std::uint8_t>(profile.name.size()));
        out.insert(out.end(), profile.name.begin(), profile.name.end());
        out.resize(prof_offset + ((out.size() - prof_offset + kProfileRecordSize - 1) / kProfileRecordSize) * kProfileRecordSize, 0);
    }
    const std::size_t stat_offset = out.size();
    for (const auto& profile : profiles_) { appendU64(out, profile.id); appendStats(out, profile.stats); }
    const auto ctrl_offset=out.size();
    for(const auto& p:profiles_) {
        appendU64(out,p.id);out.push_back(p.slot);out.push_back(p.controls_valid);
        out.insert(out.end(),p.controls.begin(),p.controls.end());out.insert(out.end(),3,0);
    }
    const auto writeDirectory = [&](std::size_t at, const char tag[4], std::size_t offset,
                                    std::size_t size, std::size_t count) {
        std::copy(tag, tag + 4, out.begin() + static_cast<std::ptrdiff_t>(at));
        patchU32(out, at + 4, static_cast<std::uint32_t>(offset));
        patchU32(out, at + 8, static_cast<std::uint32_t>(size));
        patchU32(out, at + 12, static_cast<std::uint32_t>(count));
    };
    writeDirectory(directory, "PROF", prof_offset, profiles_.size() * kProfileRecordSize, profiles_.size());
    writeDirectory(directory + kDirectoryEntrySize, "STAT", stat_offset,
                   profiles_.size() * kStatsRecordSize, profiles_.size());
    writeDirectory(directory+2*kDirectoryEntrySize,"CTRL",ctrl_offset,profiles_.size()*72,profiles_.size());
    patchU32(out, 24, static_cast<std::uint32_t>(out.size()));
    patchU32(out, 28, 0);
    patchU32(out, 28, crc32(out));
    return out;
}

void UserProfileStore::save() {
    if (path_.empty() || !loaded_) throw std::runtime_error("profile store is not safely loaded");
    if(generation_==(std::numeric_limits<uint64_t>::max)()) throw std::runtime_error("profile generation exhausted");
    const std::uint64_t next_generation = generation_ + 1;
    auto bytes=serialize(next_generation);
    auto next_backup=primary_exists_ && !recovered_backup_ ? primary_bytes_:backup_bytes_; // Allocate before commit.
    std::filesystem::create_directories(path_.parent_path());
    const ProfileWriteLock lock(path_);
    if(std::filesystem::exists(path_)!=primary_exists_ ||
       (primary_exists_ && readProfileBytes(path_)!=primary_bytes_))
        throw std::runtime_error("profile save changed on disk; reload before writing");
    const auto backup=std::filesystem::path(path_.wstring()+L".bak");
    if(backup_protected_ || std::filesystem::exists(backup)!=backup_exists_ ||
       (backup_exists_ && readProfileBytes(backup)!=backup_bytes_))
        throw std::runtime_error("profile backup is unsupported or changed; refused overwrite");
    writeAtomically(bytes);
    backup_exists_=(primary_exists_ && !recovered_backup_) || backup_exists_;backup_bytes_.swap(next_backup);
    primary_bytes_=std::move(bytes);primary_exists_=true;generation_=next_generation;recovered_backup_=false;
}

void UserProfileStore::writeAtomically(const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path_.parent_path());
    const auto temp = std::filesystem::path(path_.wstring() + L".tmp");
    const auto backup = std::filesystem::path(path_.wstring() + L".bak");
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot create profile temp file");
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) throw std::runtime_error("cannot write profile temp file");
    }
#ifdef _WIN32
    if (std::filesystem::exists(path_)) {
        if (!ReplaceFileW(path_.c_str(), temp.c_str(), recovered_backup_ ? nullptr:backup.c_str(), REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
            std::filesystem::remove(temp);
            throw std::runtime_error("atomic profile replacement failed: " + std::to_string(GetLastError()));
        }
    } else if (!MoveFileExW(temp.c_str(), path_.c_str(), MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temp);
        throw std::runtime_error("atomic profile creation failed: " + std::to_string(GetLastError()));
    }
#else
    std::error_code ignored;
    if (std::filesystem::exists(path_) && !recovered_backup_) std::filesystem::copy_file(path_, backup,
        std::filesystem::copy_options::overwrite_existing, ignored);
    std::filesystem::rename(temp, path_);
#endif
}

void UserProfileMenu::open(std::size_t profile_count) noexcept {
    selected_ = profile_count; // The recovered -1 sentinel is Start New.
    editing_ = false;
    editing_existing_ = false;
    delete_pending_ = false;
    draft_.clear();
    message_.clear();
}

bool UserProfileMenu::move(int direction, std::size_t profile_count) noexcept {
    if (editing_ || delete_pending_ || !direction) return false;
    const std::size_t previous = selected_;
    if (direction < 0 && selected_ > 0) --selected_;
    if (direction > 0 && selected_ < profile_count) ++selected_;
    return selected_ != previous;
}

void UserProfileMenu::beginEdit(const UserProfileStore& store) {
    delete_pending_ = false;
    message_.clear();
    editing_existing_ = selected_ < store.profiles().size();
    draft_ = editing_existing_ ? store.profiles()[selected_].name : std::string{};
    editing_ = true;
}

bool UserProfileMenu::append(char value) {
    if (!editing_ || draft_.size() >= kMaximumUserNameLength) return false;
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
    if (!UserProfileStore::isAllowedNameCharacter(upper)) return false;
    if (upper == ' ' && (draft_.empty() || draft_.back() == ' ')) return false;
    draft_.push_back(upper);
    message_.clear();
    return true;
}

bool UserProfileMenu::backspace() {
    if (!editing_ || draft_.empty()) return false;
    draft_.pop_back();
    message_.clear();
    return true;
}

bool UserProfileMenu::commit(UserProfileStore& store) {
    if (!editing_) return false;
    std::size_t created = 0;
    const bool changed = editing_existing_ ? store.rename(selected_, draft_)
                                           : store.create(draft_, &created);
    if (!changed) {
        message_ = store.lastError();
        return false;
    }
    if (!editing_existing_) selected_ = created;
    editing_ = false;
    editing_existing_ = false;
    draft_.clear();
    message_ = "profile saved";
    return true;
}

bool UserProfileMenu::requestDelete(UserProfileStore& store) {
    if (editing_ || selected_ >= store.profiles().size()) return false;
    if (!delete_pending_) {
        delete_pending_ = true;
        message_ = "press delete again to confirm";
        return false;
    }
    const bool changed = store.erase(selected_);
    if (selected_ > store.profiles().size()) selected_ = store.profiles().size();
    delete_pending_ = false;
    message_ = changed ? "profile deleted" : store.lastError();
    return changed;
}

void UserProfileMenu::cancel() noexcept {
    editing_ = false;
    editing_existing_ = false;
    delete_pending_ = false;
    draft_.clear();
    message_.clear();
}

} // namespace nba97
