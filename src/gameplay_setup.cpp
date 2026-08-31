#include "gameplay_setup.hpp"
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace nba97 {
namespace {
constexpr std::size_t payload_size=64+2*256*4,pack_size=20+payload_size;
std::uint32_t word(const std::vector<std::uint8_t>& b,std::size_t at) {
    return std::uint32_t(b.at(at)) | (std::uint32_t(b.at(at+1))<<8) |
        (std::uint32_t(b.at(at+2))<<16) | (std::uint32_t(b.at(at+3))<<24);
}
std::int16_t half(const std::vector<std::uint8_t>& b,std::size_t at) {
    const unsigned n=unsigned(b.at(at))|(unsigned(b.at(at+1))<<8);
    return static_cast<std::int16_t>(n<0x8000u ? int(n):int(n)-65536);
}
std::uint32_t crc(const std::uint8_t* data,std::size_t count) {
    std::uint32_t value=0xffffffffu;
    for(std::size_t i=0;i<count;++i) {
        value^=data[i];
        for(unsigned bit=0;bit<8;++bit) value=(value>>1)^((0u-(value&1u))&0xedb88320u);
    }
    return ~value;
}
}
const GameplayFormation& GameplaySetup::formation(unsigned index) const {
    return formations_.at(index);
}
std::uint32_t GameplaySetup::duration(bool overtime,std::uint8_t option) const noexcept {
    // Original65140 caller65DB0 reads an unchecked byte-indexed word. Preserve
    // the actual adjacent source data for raw options beyond the ordinary five.
    return durations_[overtime ? 1:0][option];
}
Nba97GameMotionHeaderView GameplaySetup::motionView(unsigned channel,unsigned slot) const {
    const auto* h=mocap_->header(channel,slot);
    if(!h) return {};
    //640D8 changes flags/count, but not byte+2. Never read count from raw bytes
    // or infer mode2 from another field when making a resetter's header view.
    return {h->flags,mocap_->bytes().at(std::size_t(h->header_offset)+2),h->count,1};
}
GameplaySetupResource decodeGameplaySetup(const std::vector<std::uint8_t>& b,GameplayMocapResource mocap) {
    if(!mocap) throw std::runtime_error("gameplay setup needs owned motion data");
    if(b.size()!=pack_size || std::memcmp(b.data(),"NBA97PER",8)!=0 ||
       word(b,8)!=1 || word(b,12)!=payload_size || word(b,16)!=crc(b.data()+20,payload_size))
        throw std::runtime_error("invalid private gameplay period pack");
    std::shared_ptr<GameplaySetup> next(new GameplaySetup);
    next->mocap_=std::move(mocap);
    for(unsigned table=0;table<2;++table) {
        for(unsigned player=0;player<5;++player)
            for(unsigned axis=0;axis<3;++axis)
                next->formations_[table][player][axis]=half(b,20+table*32+player*6+axis*2);
        for(unsigned option=0;option<256;++option)
            next->durations_[table][option]=word(b,20+64+(table*256+option)*4);
    }
    return next;
}
GameplaySetupResource loadGameplaySetup(const std::filesystem::path& folder) {
    std::ifstream input(folder/"period_setup.bin",std::ios::binary|std::ios::ate);
    if(!input || input.tellg()!=std::streampos(pack_size))
        throw std::runtime_error("missing or invalid private gameplay period pack");
    std::vector<std::uint8_t> bytes(pack_size);
    input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()));
    if(!input || input.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("gameplay period pack changed during read");
    return decodeGameplaySetup(bytes,load_gameplay_mocap(folder/"ZMOCAP.BIN"));
}
}
