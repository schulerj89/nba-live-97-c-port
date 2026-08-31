#include "gameplay_animation.hpp"
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace nba97 {
namespace {
constexpr std::size_t payload_size=0x30084,pack_size=20+payload_size;
std::uint32_t word(const std::vector<std::uint8_t>& bytes,std::size_t at) {
    return std::uint32_t(bytes.at(at))|(std::uint32_t(bytes.at(at+1))<<8)|
        (std::uint32_t(bytes.at(at+2))<<16)|(std::uint32_t(bytes.at(at+3))<<24);
}
std::uint32_t crc(const std::uint8_t* bytes,std::size_t count) {
    std::uint32_t value=0xffffffffu;
    for(std::size_t i=0;i<count;++i) {
        value^=bytes[i];
        for(unsigned bit=0;bit<8;++bit)value=(value>>1)^((0u-(value&1u))&0xedb88320u);
    }
    return ~value;
}
}
GameplayAnimationResource decodeGameplayAnimation(const std::vector<std::uint8_t>& bytes,
                                                  GameplaySetupResource setup) {
    if(!setup)throw std::runtime_error("animation resources need owned setup data");
    if(bytes.size()!=pack_size || std::memcmp(bytes.data(),"NBA97ANI",8)!=0 ||
       word(bytes,8)!=1 || word(bytes,12)!=payload_size ||
       word(bytes,16)!=crc(bytes.data()+20,payload_size))
        throw std::runtime_error("invalid private gameplay animation pack");
    std::shared_ptr<GameplayAnimation> next(new GameplayAnimation);
    next->setup_=std::move(setup);next->words_.resize(payload_size/2);
    for(std::size_t i=0;i<next->words_.size();++i)
        next->words_[i]=std::uint16_t(unsigned(bytes[20+i*2])|(unsigned(bytes[21+i*2])<<8));
    //6CFE0/706E4 use separate byte tables within the same owned GAME window.
    // Decode signed boundary bytes explicitly; never borrow host-endian words.
    for(unsigned i=0;i<257;++i)next->direction_[i]=bytes[20+0xd72b4-0xa850c+i];
    next->physics_.direction={next->direction_.data(),next->direction_.size()};
    for(unsigned side=0;side<2;++side) {
        for(unsigned i=0;i<8;++i) {
            const unsigned value=bytes[20+0xb8a54-0xa850c+side*8+i];
            next->boundary_[side][i]=std::int8_t(value<128?int(value):int(value)-256);
        }
        next->physics_.boundary[side]=next->boundary_[side].data();
        next->physics_.boundary_count[side]=next->boundary_[side].size();
    }
    constexpr unsigned source[]={0xb850c,0xb8538,0xb8564,0xb8590,0xb85bc,0xb85e8,0xb8614};
    for(unsigned i=0;i<7;++i) {
        const bool initial=i==2 || i==3;
        const auto offset=source[i]-0xa850cu-(initial?0u:0x10000u);
        auto& map=next->view_.map[i];
        map.words=next->words_.data()+offset/2;map.count=65536;
        map.first_index=initial?0:-32768;
    }
    const auto& mocap=next->setup_->mocap();
    for(unsigned channel=0;channel<2;++channel)for(unsigned slot=0;slot<84;++slot) {
        auto& clip=next->view_.clip[channel][slot];
        clip.header=next->setup_->motionView(channel,slot);
        if(const auto* header=mocap->header(channel,slot))
            clip.step3=header->timing; //640D8 halves byte3 when flag8 first normalizes.
    }
    return next;
}
GameplayAnimationResource loadGameplayAnimation(const std::filesystem::path& path,
                                                GameplaySetupResource setup) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input || input.tellg()!=std::streampos(pack_size))
        throw std::runtime_error("missing or invalid private gameplay animation pack");
    std::vector<std::uint8_t> bytes(pack_size);
    input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()));
    if(!input || input.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("gameplay animation pack changed during read");
    return decodeGameplayAnimation(bytes,std::move(setup));
}
}
