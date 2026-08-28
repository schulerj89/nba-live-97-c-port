#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace nba97 {
struct CoolFactEntry {
    std::uint32_t logical_record, physical_record, bytes, offset;
};
// 315BC: logical=player*5+variant; physical=logical+1 (zero is fallback).
// IDX's count excludes its reserved record zero. Original trailers are retained.
class CoolFactIndexView {
public:
    explicit CoolFactIndexView(const std::vector<std::uint8_t>& bytes):bytes_(bytes) {
        if(bytes.size()<12) throw std::runtime_error("missing/truncated Z1COOL.IDX");
        count_=word(0);
        if(count_>(bytes.size()-12)/8) throw std::runtime_error("Z1COOL.IDX count excludes reserved record zero; table truncated");
    }
    std::uint32_t count() const noexcept {return count_;}
    CoolFactEntry lookup(std::uint16_t player,std::uint32_t variant) const {
        if(variant>=5) throw std::runtime_error("Cool Fact variant outside0..4");
        const auto logical=std::uint32_t(player)*5+variant;
        const auto physical=logical<count_ ? logical+1 : 0;
        const auto at=4+std::size_t(physical)*8;
        return {logical,physical,word(at),word(at+4)};
    }
private:
    std::uint32_t word(std::size_t at) const noexcept {
        return std::uint32_t(bytes_[at])|(std::uint32_t(bytes_[at+1])<<8)|
            (std::uint32_t(bytes_[at+2])<<16)|(std::uint32_t(bytes_[at+3])<<24);
    }
    const std::vector<std::uint8_t>& bytes_;
    std::uint32_t count_=0;
};
}
