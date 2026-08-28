#include "roster_save_codec.hpp"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <limits>
#include <new>

// Test-only requested-allocation accounting. Excludes allocator bookkeeping,
// stack, pre-existing catalogue/document and rendering. Epochs allow an output
// to outlive a measured call without corrupting the next measurement.
namespace allocation_probe {
struct alignas(std::max_align_t) Header { std::size_t bytes,epoch; };
std::size_t epoch=0,current=0,peak=0;
bool active=false;
void begin() { ++epoch; current=peak=0; active=true; }
std::size_t end() { active=false; return peak; }
}
void* operator new(std::size_t n) {
    using namespace allocation_probe;
    if(n>std::numeric_limits<std::size_t>::max()-sizeof(Header)) throw std::bad_alloc();
    auto* h=static_cast<Header*>(std::malloc(sizeof(Header)+(n ? n : 1)));
    if(!h) throw std::bad_alloc();
    h->bytes=n; h->epoch=active ? epoch : 0;
    if(active) { current+=n; peak=(std::max)(peak,current); }
    return h+1;
}
void operator delete(void* p) noexcept {
    if(!p) return;
    using namespace allocation_probe;
    auto* h=static_cast<Header*>(p)-1;
    if(active && h->epoch==epoch) current-=h->bytes;
    std::free(h);
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }

namespace {
using namespace nba97;
using Bytes=std::vector<std::uint8_t>;
using Kind=RosterSaveErrorKind;
void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
void pass(const char* name) { std::cout<<"SAVE-CODEC PASS "<<name<<'\n'; }
void set(Bytes& b,std::size_t at,std::uint64_t v,unsigned n=4) {
    check(at+n<=b.size(),"bad test offset");
    for(unsigned i=0;i<n;++i) { b[at+i]=static_cast<std::uint8_t>(v); v>>=8; }
}
void seal(Bytes& b) { set(b,b.size()-4,rosterSaveCrc32(b.data(),b.size()-4)); }
void sectionSeal(Bytes& b,std::size_t at,std::size_t n) {
    set(b,at+12,rosterSaveCrc32(b.data()+at+16,n)); seal(b);
}
template<class F> void rejects(F run,Kind kind) {
    try { run(); } catch(const RosterSaveError& e) { check(e.kind()==kind,"wrong rejection category"); return; }
    throw std::runtime_error("invalid save accepted");
}
RosterSlots baseline() {
    RosterSlots b; b.fill(UINT16_MAX);
    unsigned id=1000;
    for(unsigned team=0;team<29;++team) for(unsigned i=0;i<13;++i) b[team*15+i]=static_cast<std::uint16_t>(id++);
    for(unsigned i=0;i<12;++i) b[435+i]=static_cast<std::uint16_t>(id++);
    return b;
}
void tests() {
    const auto base=baseline();
    RosterBaseIdentity identity{}; for(unsigned i=0;i<32;++i) identity[i]=static_cast<std::uint8_t>(i+1);
    RosterSaveDocument doc; doc.slots=base; doc.generation=1;
    const auto empty=encodeRosterSave(doc,base,identity);
    check(empty.size()==68 && decodeRosterSave(empty,base,identity).slots==base,"empty override round trip");
    // Frozen independently with Python struct.pack + zlib.crc32, not by this
    // codec. Keep this v1 fixture when adding migrations/new format versions.
    const char* golden="4e3937524f535400010000004000000044000000000000000100000000000000"
        "0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f200435bbab";
    const auto digit=[](char c) {return c<='9' ? c-'0' : c-'a'+10;};
    for(std::size_t i=0;i<empty.size();++i)
        check(empty[i]==digit(golden[i*2])*16+digit(golden[i*2+1]),"frozen v1 wire fixture changed");
    check(rosterSaveCrc32(reinterpret_cast<const std::uint8_t*>("123456789"),9)==0xcbf43926 &&
          rosterSaveCrc32(nullptr,0)==0,"CRC32 published check vector");
    check(empty[8]==1 && empty[12]==64 && empty[16]==68 && empty[24]==1 && empty[32]==1,
          "header layout changed");
    pass("empty_override_and_explicit_wire_header");

    std::swap(doc.slots[0],doc.slots[1]);
    const auto one=encodeRosterSave(doc,base,identity);
    check(one.size()==148 && std::memcmp(one.data()+64,"TEAM",4)==0 && one[80]==0 && one[82]==15 &&
          one[84]==0xe9 && one[85]==3 && one[86]==0 && one[87]==0 &&
          std::all_of(one.begin()+136,one.begin()+144,[](auto b){return b==255;}),"list wire IDs/sentinel");
    check(decodeRosterSave(one,base,identity).slots==doc.slots && encodeRosterSave(doc,base,identity)==one,
          "one-team exact deterministic round trip");
    for(unsigned team=1;team<29;++team) std::swap(doc.slots[team*15],doc.slots[team*15+1]);
    std::swap(doc.slots[435],doc.slots[436]);
    const auto full=encodeRosterSave(doc,base,identity);
    check(full.size()==2360 && std::memcmp(full.data()+64,"FREE",4)==0 &&
          decodeRosterSave(full,base,identity).slots==doc.slots,"all lists round trip");
    allocation_probe::begin();
    const auto measured_bytes=encodeRosterSave(doc,base,identity);
    const auto encode_peak=allocation_probe::end();
    allocation_probe::begin();
    const auto measured_doc=decodeRosterSave(measured_bytes,base,identity);
    const auto decode_peak=allocation_probe::end();
    check(measured_doc.slots==doc.slots && encode_peak<16*1024 && decode_peak<16*1024,
          "roster-only codec requested heap budget exceeded");
    // Membership is conserved globally, not per team. Trade can reuse the
    // container later; feature-level Re-order still imposes per-team rules.
    std::swap(doc.slots[0],doc.slots[435]);
    check(decodeRosterSave(encodeRosterSave(doc,base,identity),base,identity).slots==doc.slots,"cross-list population");
    pass("one_all_and_cross_list_round_trips");

    doc.slots=base; doc.generation=std::numeric_limits<std::uint64_t>::max(); doc.minor_version=19;
    doc.extensions={{{'Z','Z','Z','Z'},7,{0,255,1,0}},{{'A','A','A','A'},0,{}}};
    const auto extended=encodeRosterSave(doc,base,identity);
    const auto restored=decodeRosterSave(extended,base,identity);
    check(restored.generation==doc.generation && restored.minor_version==19 && restored.extensions.size()==2 &&
          restored.extensions[0].version==0 && restored.extensions[1].payload==doc.extensions[0].payload &&
          encodeRosterSave(restored,base,identity)==extended,"optional section/version preservation");
    pass("optional_data_minor_version_and_generation_preserved");

    auto rejectBytes=[&](Bytes b,Kind kind) { rejects([&]{decodeRosterSave(b,base,identity);},kind); };
    for(auto offset : {8u,12u,14u,22u}) { auto b=one; set(b,offset,99,2); seal(b); rejectBytes(b,Kind::Unsupported); }
    for(auto offset : {68u,70u}) { auto b=one; set(b,offset,2,2); seal(b); rejectBytes(b,Kind::Unsupported); }
    auto b=extended; set(b,70,1,2); seal(b); rejectBytes(b,Kind::Unsupported); // unknown required
    auto wrong_identity=identity; wrong_identity[3]^=1;
    rejects([&]{decodeRosterSave(one,base,wrong_identity);},Kind::WrongBase);
    pass("newer_required_formats_and_wrong_base_fail_closed");

    for(auto offset : {16u,20u,24u}) { b=one; set(b,offset,0,offset==20 ? 2 : 4); seal(b); rejectBytes(b,Kind::Corrupt); }
    b=one; set(b,20,65,2); seal(b); rejectBytes(b,Kind::Corrupt);
    b=one; set(b,72,UINT32_MAX); seal(b); rejectBytes(b,Kind::Corrupt);
    b=one; b.insert(b.end()-4,0); set(b,16,b.size()); seal(b); rejectBytes(b,Kind::Corrupt);
    b=one; b[64]='?'; seal(b); rejectBytes(b,Kind::Corrupt);
    b=one; b[84]^=1; seal(b); rejectBytes(b,Kind::Corrupt); // inner CRC despite valid outer
    pass("length_count_tag_and_nested_checksum_guards");

    for(auto value : {29u,65535u}) { b=one; set(b,80,value,2); sectionSeal(b,64,64); rejectBytes(b,Kind::Corrupt); }
    b=one; set(b,82,14,2); sectionSeal(b,64,64); rejectBytes(b,Kind::Corrupt);
    for(auto id : {0x8000u,0xffffu,0x10000u,0xfffffffeu,999u,1000u,UINT32_MAX}) {
        b=one; set(b,84,id); sectionSeal(b,64,64); rejectBytes(b,Kind::InvalidRoster);
    }
    b=full; set(b,80,28,2); sectionSeal(b,64,404); rejectBytes(b,Kind::Corrupt);
    b=full; set(b,82,99,2); sectionSeal(b,64,404); rejectBytes(b,Kind::Corrupt);
    b=full; set(b,500+64,0,2); sectionSeal(b,484,29*64); rejectBytes(b,Kind::Corrupt); // duplicate team
    pass("list_descriptors_and_nontruncating_population_guards");

    b=extended; std::copy_n(b.begin()+64,4,b.begin()+80); seal(b); rejectBytes(b,Kind::Corrupt);
    b=extended; std::fill_n(b.begin()+64,4,'Z'); std::fill_n(b.begin()+80,4,'A'); seal(b); rejectBytes(b,Kind::Corrupt);
    auto invalid=doc; invalid.extensions.push_back(invalid.extensions[0]);
    rejects([&]{encodeRosterSave(invalid,base,identity);},Kind::Corrupt);
    invalid=doc; invalid.extensions[0].tag={'T','E','A','M'};
    rejects([&]{encodeRosterSave(invalid,base,identity);},Kind::Corrupt);
    invalid=doc; invalid.extensions.resize(65);
    rejects([&]{encodeRosterSave(invalid,base,identity);},Kind::Corrupt);
    invalid=doc; invalid.generation=0;
    rejects([&]{encodeRosterSave(invalid,base,identity);},Kind::Corrupt);
    invalid=doc; invalid.slots[0]=invalid.slots[1];
    rejects([&]{encodeRosterSave(invalid,base,identity);},Kind::InvalidRoster);
    auto bad_base=base; bad_base[0]=bad_base[1]; invalid.slots=bad_base;
    rejects([&]{encodeRosterSave(invalid,bad_base,identity);},Kind::InvalidRoster);
    pass("noncanonical_duplicate_reserved_and_invalid_encode_rejected");

    invalid=doc; invalid.extensions={{{'D','A','T','A'},1,Bytes(kRosterSaveMaxBytes-84,0)}};
    const auto limit=encodeRosterSave(invalid,base,identity);
    check(limit.size()==kRosterSaveMaxBytes && decodeRosterSave(limit,base,identity).extensions[0].payload.size()==kRosterSaveMaxBytes-84,
          "exact size cap rejected");
    invalid.extensions.push_back({{'N','E','X','T'},1,{}});
    rejects([&]{encodeRosterSave(invalid,base,identity);},Kind::Corrupt); // subtraction underflow regression
    invalid.extensions.pop_back(); invalid.extensions[0].payload.push_back(0);
    rejects([&]{encodeRosterSave(invalid,base,identity);},Kind::Corrupt);
    rejectBytes(Bytes(kRosterSaveMaxBytes+1,0),Kind::Corrupt);
    pass("exact_size_cap_and_overflow_boundaries");

    unsigned truncated=0,bit_flips=0;
    for(std::size_t n=0;n<one.size();++n) { rejectBytes(Bytes(one.begin(),one.begin()+n),Kind::Corrupt); ++truncated; }
    // Some header-bit changes intentionally classify as Unsupported rather
    // than Corrupt. Either must refuse; never catch arbitrary exceptions.
    for(std::size_t i=0;i<one.size();++i) for(unsigned bit=0;bit<8;++bit) {
        b=one; b[i]^=static_cast<std::uint8_t>(1u<<bit);
        bool refused=false; try { decodeRosterSave(b,base,identity); } catch(const RosterSaveError&) {refused=true;}
        check(refused,"single-bit corruption accepted"); ++bit_flips;
    }
    pass("every_truncation_and_single_bit_corruption_rejected");

    std::uint32_t rng=0x97c0dec;
    unsigned rejected=0,accepted=0;
    for(unsigned iteration=0;iteration<4096;++iteration) {
        auto next=[&] { rng=rng*1664525u+1013904223u; return rng; };
        b=full;
        for(unsigned j=0,n=1+next()%8;j<n;++j) { const auto at=next()%b.size(); b[at]^=static_cast<std::uint8_t>(1+next()%255); }
        // Repair checksums to reach parser and membership logic, not only CRC.
        sectionSeal(b,64,404); sectionSeal(b,484,29*64);
        try {
            const auto parsed=decodeRosterSave(b,base,identity);
            auto sorted=parsed.slots, original=base;
            std::sort(sorted.begin(),sorted.end()); std::sort(original.begin(),original.end());
            check(sorted==original,"mutation accepted changed population");
            check(decodeRosterSave(encodeRosterSave(parsed,base,identity),base,identity).slots==parsed.slots,
                  "accepted mutation failed valid round trip");
            ++accepted;
        } catch(const RosterSaveError&) { ++rejected; }
    }
    check(rejected>4000,"mutation fixture did not stress rejected structures");
    pass("seeded_resealed_structural_mutations");
    std::cout<<"SAVE-CODEC SUMMARY checks=10 truncations="<<truncated<<" bit_flips="<<bit_flips
             <<" resealed_mutations=4096 rejected="<<rejected<<" accepted_valid="<<accepted
             <<" default_bytes=68 one_team_bytes=148 all_lists_bytes=2360 slot_table_bytes="<<sizeof(RosterSlots)
             <<" encode_requested_heap_peak="<<encode_peak<<" decode_requested_heap_peak="<<decode_peak
             <<" memory_scope=codec_only_excludes_stack_allocator_catalogue disk_io=none catalogue_identity=caller_supplied\n";
}
}
int main() {
    try { tests(); return 0; } catch(const std::exception& e) { std::cerr<<"SAVE-CODEC FAIL "<<e.what()<<'\n'; return 1; }
}
