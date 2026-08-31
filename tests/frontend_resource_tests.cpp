#include "recovered/frontend_resource.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

static void check(bool value) { if (!value) std::abort(); }
struct Fixture {
    Nba97CoolIndexLoad s{0,1,99,17,23,31,41,51};
    uint32_t index=61;
    unsigned status_calls=0, load_pumps=0;
    bool already_finished=false;
    std::vector<std::array<uint32_t,4>> calls;
};
static uint32_t invoke(void* context,Nba97CoolIndexCall kind,uint32_t a,uint32_t b,uint32_t c) {
    auto& f=*static_cast<Fixture*>(context);
    f.calls.push_back({static_cast<uint32_t>(kind),a,b,c});
    switch(kind) {
    case NBA97_COOL_INDEX_REQUEST: f.s.loaded_data=0; break;
    case NBA97_COOL_INDEX_PUMP_UI:
    case NBA97_COOL_INDEX_PUMP_IO:
        if (!f.s.loaded_data && ++f.load_pumps==2) f.s.loaded_data=71;
        break;
    case NBA97_COOL_INDEX_VOICE_STATUS:
        check(f.index==71 && f.s.pending==0);
        return f.already_finished || ++f.status_calls>=3;
    default: break;
    }
    return 0;
}
int main() {
    uint16_t crc=0;
    const uint8_t bytes[]={'1','2','3','4','5','6','7','8','9'};
    // Literal checksum from original9045C execution, not this implementation.
    check(nba97_resource_crc16(bytes,9,&crc)==1 && crc==0x28c4);
    check(nba97_resource_crc16(nullptr,0,&crc)==1 && crc==0xfbea);
    crc=123; check(!nba97_resource_crc16(nullptr,1,&crc) && crc==123);
    check(!nba97_resource_crc16(bytes,0x80000000u,&crc) && crc==123);
    std::vector<uint8_t> portrait(bytes,bytes+9);
    portrait.push_back(0xc4);portrait.push_back(0x28);
    uint32_t blocked=77;
    check(nba97_portrait_checksum_accept(portrait.data(),11,&blocked)==1 && !blocked);
    portrait[0]^=1;blocked=77;
    check(nba97_portrait_checksum_accept(portrait.data(),11,&blocked)==0 && blocked==77);
    check(nba97_portrait_checksum_accept(nullptr,11,&blocked)==-1 && blocked==77);
    portrait[0]^=1;
    // The embedded little-endian CRC makes the full payload CRC zero. The
    // outer CRCF stores32 bits, and its nominal length is deliberately wrong.
    portrait.insert(portrait.end(),{'C','R','C','F',99,88,77,66,0,0,0,0});
    Nba97ResourceValidation v{};
    check(nba97_resource_validate_file(portrait.data(),23,1,&v)==1);
    check(v.payload_bytes==11 && v.trailer_present && v.calculated_checksum==0);
    portrait[21]=1;
    check(nba97_resource_validate_file(portrait.data(),23,0,&v)==0);
    check(v.stored_checksum==65536);
    check(nba97_resource_validate_file(bytes,9,0,&v)==1 && !v.trailer_present);
    check(nba97_resource_validate_file(bytes,9,1,&v)==0);
    check(nba97_resource_validate_file(nullptr,0,0,&v)==1);
    v.payload_bytes=123;
    check(nba97_resource_validate_file(nullptr,9,0,&v)==-1 && v.payload_bytes==123);
    for (bool graphics : {false,true}) {
        Fixture f; f.s.graphics=graphics;
        check(nba97_cool_index_load(&f.s,&f.index,81,91,invoke,&f));
        check(f.index==71 && f.s.archive_path==91 && !f.s.sample_data);
        check(f.s.voice==UINT32_MAX && f.s.bank==UINT32_MAX && f.status_calls==3);
        check(f.calls.front()==std::array<uint32_t,4>{NBA97_COOL_INDEX_FREE_DATA,61,0,0});
        check(f.calls.back()==std::array<uint32_t,4>{NBA97_COOL_INDEX_FREE_DATA,31,0,0});
        unsigned pumps=0;for(const auto& call:f.calls)
            if(call[0]==NBA97_COOL_INDEX_PUMP_UI || call[0]==NBA97_COOL_INDEX_PUMP_IO) ++pumps;
        check(pumps==(graphics?3u:2u)); // No I/O pump during source no-graphics voice wait.
    }
    Fixture done;done.already_finished=true;
    check(nba97_cool_index_load(&done.s,&done.index,81,91,invoke,&done));
    check(done.s.voice==17); // Source stale already-finished voice survives.
    Fixture absent;
    check(nba97_cool_index_load(&absent.s,&absent.index,0,91,invoke,&absent));
    check(!absent.index && absent.s.archive_path==51 && absent.s.voice==17 &&
        absent.s.sample_data==31 && absent.calls.size()==1);
    Fixture refused;
    check(!nba97_cool_index_load(&refused.s,nullptr,81,91,invoke,&refused));
    check(refused.calls.empty() && refused.index==61);
    std::cout << "frontend resource checksum and ownership tests passed\n";
}
