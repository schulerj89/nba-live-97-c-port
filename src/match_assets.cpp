#include "match_assets.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace nba97 {
std::array<uint8_t,59> loadMatchControlDefaults(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file || file.tellg()!=71)
        throw std::runtime_error("missing/bounded match controls; run tools/extract_match_setup.py");
    std::array<uint8_t,71> bytes{};file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    if(!file || !std::equal(bytes.begin(),bytes.begin()+4,"N97C") ||
       bytes[4]!=1 || bytes[5] || bytes[6]!=59 || bytes[7] ||
       bytes[8]!=0xd8 || bytes[9]!=0x1c || bytes[10]!=0x0c || bytes[11]!=0x80)
        throw std::runtime_error("unsupported match control default pack");
    std::array<uint8_t,59> result;std::copy_n(bytes.begin()+12,59,result.begin());return result;
}
}
