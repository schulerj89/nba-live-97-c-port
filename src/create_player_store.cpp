#include "create_player_store.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace nba97 {
namespace {
constexpr std::uint8_t kMagic[8]{'N','9','7','C','P','L','R',0};
constexpr std::uint16_t kMajor = 1, kMinor = 2;
constexpr std::size_t kHeader = 32;
constexpr std::size_t kV10SlotBytes = NBA97_CREATED_PLAYER_RECORD_SIZE + 12 + 16 + 1;
constexpr std::size_t kV11SlotBytes = kV10SlotBytes + 1;
constexpr std::size_t kSlotBytes = NBA97_CREATED_PLAYER_RECORD_SIZE + 13 + 13 + 2;
constexpr std::size_t kPayload = NBA97_CREATED_PLAYER_CAPACITY * kSlotBytes;

void u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value)); out.push_back(static_cast<std::uint8_t>(value >> 8));
}
void u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift=0; shift<32; shift+=8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift=0; shift<64; shift+=8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}
std::uint16_t get16(const std::vector<std::uint8_t>& in,std::size_t at) {
    if(at+2>in.size()) throw std::runtime_error("truncated created-player u16");
    return static_cast<std::uint16_t>(in[at] | (in[at+1]<<8));
}
std::uint32_t get32(const std::vector<std::uint8_t>& in,std::size_t at) {
    if(at+4>in.size()) throw std::runtime_error("truncated created-player u32");
    std::uint32_t v=0; for(int shift=0;shift<32;shift+=8)v|=std::uint32_t(in[at++])<<shift; return v;
}
std::uint64_t get64(const std::vector<std::uint8_t>& in,std::size_t at) {
    if(at+8>in.size()) throw std::runtime_error("truncated created-player u64");
    std::uint64_t v=0; for(int shift=0;shift<64;shift+=8)v|=std::uint64_t(in[at++])<<shift; return v;
}
void patch32(std::vector<std::uint8_t>& out,std::size_t at,std::uint32_t value) {
    for(int shift=0;shift<32;shift+=8)out[at++]=static_cast<std::uint8_t>(value>>shift);
}
std::uint32_t crc32(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t value=0xffffffffu;
    for(auto byte:bytes){value^=byte;for(int bit=0;bit<8;++bit)value=(value>>1)^(0xedb88320u&(0u-(value&1u)));}
    return ~value;
}
bool terminated(const char* text,std::size_t size) { return std::memchr(text,'\0',size)!=nullptr; }
void validate(const Nba97CreatedPlayerCatalog& catalog) {
    for(int slot=0;slot<NBA97_CREATED_PLAYER_CAPACITY;++slot) {
        const auto id=nba97_created_player_id(&catalog.records[slot]);
        if(id!=UINT16_MAX && id!=NBA97_CREATED_PLAYER_FIRST_ID+slot)
            throw std::runtime_error("created-player slot has invalid stable ID");
        const auto& meta=catalog.metadata[slot];
        if(!terminated(meta.first_name,sizeof(meta.first_name)) || !terminated(meta.last_name,sizeof(meta.last_name)))
            throw std::runtime_error("created-player name is not terminated");
        if(id!=UINT16_MAX && (!meta.first_name[0] || !meta.last_name[0] || meta.team>28 ||
           (meta.roster_slot!=UINT8_MAX && meta.roster_slot>99)))
            throw std::runtime_error("occupied created-player metadata is invalid");
    }
}
}

CreatedPlayerLoadStatus CreatedPlayerStore::load(const std::filesystem::path& path,
                                                  Nba97CreatedPlayerCatalog& catalog) {
    path_=std::filesystem::absolute(path).lexically_normal(); generation_=0; loaded_=false;
    nba97_created_catalog_init(&catalog);
    if(!std::filesystem::exists(path_)) { accepted_=catalog; loaded_=true; return CreatedPlayerLoadStatus::NewStore; }
    std::string primary_error; std::uint64_t next_generation=0;
    Nba97CreatedPlayerCatalog next{};
    if(readFile(path_,next,next_generation,primary_error)) {
        catalog=accepted_=next; generation_=next_generation; loaded_=true; return CreatedPlayerLoadStatus::Loaded;
    }
    const auto backup=std::filesystem::path(path_.wstring()+L".bak");
    std::string backup_error;
    if(std::filesystem::exists(backup) && readFile(backup,next,next_generation,backup_error)) {
        catalog=accepted_=next; generation_=next_generation; loaded_=true; return CreatedPlayerLoadStatus::RecoveredBackup;
    }
    throw std::runtime_error("created-player save is invalid: "+primary_error+
                             (backup_error.empty()?"":"; backup: "+backup_error));
}

bool CreatedPlayerStore::readFile(const std::filesystem::path& path,Nba97CreatedPlayerCatalog& catalog,
                                  std::uint64_t& generation,std::string& error) const {
    try {
        std::ifstream input(path,std::ios::binary);
        if(!input) throw std::runtime_error("cannot open file");
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        if(bytes.size()<kHeader || !std::equal(std::begin(kMagic),std::end(kMagic),bytes.begin()))
            throw std::runtime_error("bad magic or file size");
        if(get16(bytes,8)!=kMajor) throw std::runtime_error("unsupported major version");
        const auto payload=get32(bytes,20);
        const auto v10_payload=NBA97_CREATED_PLAYER_CAPACITY*kV10SlotBytes;
        const auto v11_payload=NBA97_CREATED_PLAYER_CAPACITY*kV11SlotBytes;
        if((payload!=kPayload && payload!=v10_payload && payload!=v11_payload) || bytes.size()!=kHeader+payload)
            throw std::runtime_error("payload size mismatch");
        const auto stored=get32(bytes,24); patch32(bytes,24,0);
        if(crc32(bytes)!=stored) throw std::runtime_error("CRC32 mismatch");
        generation=get64(bytes,12); if(!generation) throw std::runtime_error("zero durable generation");
        nba97_created_catalog_init(&catalog);
        std::size_t at=kHeader;
        for(int slot=0;slot<NBA97_CREATED_PLAYER_CAPACITY;++slot) {
            std::memcpy(catalog.records[slot].raw,bytes.data()+at,NBA97_CREATED_PLAYER_RECORD_SIZE); at+=NBA97_CREATED_PLAYER_RECORD_SIZE;
            if(payload==kPayload) {
                std::memcpy(catalog.metadata[slot].first_name,bytes.data()+at,13); at+=13;
                std::memcpy(catalog.metadata[slot].last_name,bytes.data()+at,13); at+=13;
            } else {
                std::memcpy(catalog.metadata[slot].first_name,bytes.data()+at,12); at+=12;
                std::memcpy(catalog.metadata[slot].last_name,bytes.data()+at,13); at+=16;
                catalog.metadata[slot].first_name[12]='\0';
                catalog.metadata[slot].last_name[12]='\0';
            }
            catalog.metadata[slot].team=bytes[at++];
            catalog.metadata[slot].roster_slot=payload==v10_payload ? UINT8_MAX : bytes[at++];
        }
        validate(catalog); error.clear(); return true;
    } catch(const std::exception& problem) { error=problem.what(); return false; }
}

std::vector<std::uint8_t> CreatedPlayerStore::serialize(const Nba97CreatedPlayerCatalog& catalog,
                                                        std::uint64_t generation) const {
    validate(catalog);
    std::vector<std::uint8_t> out; out.insert(out.end(),std::begin(kMagic),std::end(kMagic));
    u16(out,kMajor);u16(out,kMinor);u64(out,generation);u32(out,kPayload);u32(out,0);u32(out,0);
    for(int slot=0;slot<NBA97_CREATED_PLAYER_CAPACITY;++slot) {
        const auto& record=catalog.records[slot]; const auto& meta=catalog.metadata[slot];
        out.insert(out.end(),record.raw,record.raw+sizeof(record.raw));
        out.insert(out.end(),meta.first_name,meta.first_name+sizeof(meta.first_name));
        out.insert(out.end(),meta.last_name,meta.last_name+sizeof(meta.last_name));
        out.push_back(meta.team); out.push_back(meta.roster_slot);
    }
    patch32(out,24,crc32(out)); return out;
}

bool CreatedPlayerStore::save(const Nba97CreatedPlayerCatalog& catalog) {
    if(!loaded_ || path_.empty()) throw std::runtime_error("created-player store must be loaded before save");
    if(!std::memcmp(&catalog,&accepted_,sizeof(catalog))) return false;
    if(generation_==std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("created-player generation exhausted");
    const auto next=generation_+1; writeAtomically(serialize(catalog,next)); accepted_=catalog; generation_=next; return true;
}

void CreatedPlayerStore::writeAtomically(const std::vector<std::uint8_t>& bytes) const {
    std::filesystem::create_directories(path_.parent_path());
    const auto temp=std::filesystem::path(path_.wstring()+L".tmp");
    const auto backup=std::filesystem::path(path_.wstring()+L".bak");
    { std::ofstream output(temp,std::ios::binary|std::ios::trunc); if(!output)throw std::runtime_error("cannot create created-player temporary file");
      output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));output.flush();
      if(!output)throw std::runtime_error("cannot write created-player temporary file"); }
#ifdef _WIN32
    if(std::filesystem::exists(path_)) {
        if(!ReplaceFileW(path_.c_str(),temp.c_str(),backup.c_str(),REPLACEFILE_WRITE_THROUGH,nullptr,nullptr)) {
            std::filesystem::remove(temp); throw std::runtime_error("atomic created-player replacement failed");
        }
    } else if(!MoveFileExW(temp.c_str(),path_.c_str(),MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temp); throw std::runtime_error("atomic created-player creation failed");
    }
#else
    std::error_code ignored;
    if(std::filesystem::exists(path_))std::filesystem::copy_file(path_,backup,std::filesystem::copy_options::overwrite_existing,ignored);
    std::filesystem::rename(temp,path_);
#endif
}
} // namespace nba97
