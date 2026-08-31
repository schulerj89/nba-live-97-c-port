#include "player_portrait_archive.hpp"
#include "recovered/frontend_resource.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
void check(bool ok) { if (!ok) std::abort(); }
template<class F> void rejects(F&& f) {
    bool rejected=false;try { f(); } catch(const std::runtime_error&) { rejected=true; }
    check(rejected);
}
void word(std::vector<std::uint8_t>& b,std::uint32_t value) {
    for(unsigned shift=0;shift<32;shift+=8) b.push_back(static_cast<std::uint8_t>(value>>shift));
}
struct Fixture {
    std::vector<std::uint8_t> index, archive;
    Fixture() {
        word(index,2); // Two logical players plus physical fallback0.
        for(std::uint8_t record=0;record<3;++record) {
            std::vector<std::uint8_t> data{record,static_cast<std::uint8_t>(record+7),99};
            std::uint16_t crc=0;check(nba97_resource_crc16(data.data(),3,&crc)!=0);
            data.push_back(static_cast<std::uint8_t>(crc));data.push_back(static_cast<std::uint8_t>(crc>>8));
            word(index,static_cast<std::uint32_t>(data.size()));word(index,static_cast<std::uint32_t>(archive.size()));
            archive.insert(archive.end(),data.begin(),data.end());
        }
    }
    void trailer() {
        std::uint16_t crc=0;check(nba97_resource_crc16(index.data(),static_cast<std::uint32_t>(index.size()),&crc)!=0);
        index.insert(index.end(),{'C','R','C','F'});word(index,12);word(index,crc);
    }
};
}
int main(int argc,char** argv) {
    using Archive=nba97::PlayerPortraitArchive;
    static_assert(std::is_same_v<decltype(Archive::fromBytes({},{})),std::shared_ptr<const Archive>>);
    Fixture f;f.trailer();
    const auto archive=Archive::fromBytes(f.index,f.archive);
    check(archive->count()==2 && archive->indexBytes()==28 && archive->archiveBytes()==15);
    check(archive->physicalRecord(0)==1 && archive->physicalRecord(1)==2);
    check(archive->physicalRecord(2)==0 && archive->physicalRecord(65535)==0);
    rejects([&]{archive->physicalRecord(-1);});rejects([&]{archive->slice(3);});
    for(unsigned p=0;p<3;++p) {
        const auto raw=archive->slice(p);
        check(raw.physical_record==p && raw.offset==p*5 && raw.bytes==5 && raw.data[0]==p);
        check(archive->checksumAccepted(p));std::uint32_t blocked=17;
        check(archive->acceptChecksum(p,&blocked)==1 && !blocked);
    }
    // Construction owns immutable copies, not the caller's mutable vectors.
    f.index[0]=99;f.archive[0]^=1;
    check(archive->count()==2 && archive->checksumAccepted(0));
    Fixture corrupt;corrupt.archive.back()^=1;
    const auto invalid=Archive::fromBytes(corrupt.index,corrupt.archive);
    check(!invalid->checksumAccepted(2) && invalid->checksumAccepted(0));
    std::uint32_t blocked=17;check(invalid->acceptChecksum(2,&blocked)==0 && blocked==17);
    Fixture bad_index;bad_index.trailer();bad_index.index.back()^=1;
    rejects([&]{Archive::fromBytes(bad_index.index,bad_index.archive);});
    for(unsigned bytes : {0u,3u,4u,11u,27u}) {
        Fixture short_index;short_index.index.resize(bytes);
        rejects([&]{Archive::fromBytes(short_index.index,short_index.archive);});
    }
    Fixture last_missing;last_missing.archive.pop_back();
    rejects([&]{Archive::fromBytes(last_missing.index,last_missing.archive);});
    Fixture overflow;overflow.index[8]=0xff;overflow.index[9]=0xff;overflow.index[10]=0xff;overflow.index[11]=0xff;
    rejects([&]{Archive::fromBytes(overflow.index,overflow.archive);});
    if(argc==2) {
        const auto root=std::filesystem::path(argv[1]);
        const auto real=Archive::load(root/"Z1PORT.IDX",root/"Z1PORT.BIG");
        check(real->count()==493 && real->indexBytes()==3958 && real->archiveBytes()==13296378);
        check(real->physicalRecord(492)==493 && real->physicalRecord(493)==0);
        for(unsigned record=0;record<=real->count();++record) check(real->checksumAccepted(record));
        std::cout<<"All494 original private portrait records accepted\n";
    }
    std::cout<<"Portrait index bounds, raw checksum, ownership and source mapping passed\n";
}
