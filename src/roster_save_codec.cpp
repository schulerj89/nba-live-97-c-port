#include "roster_save_codec.hpp"
#include <algorithm>

namespace nba97 {
std::uint32_t rosterSaveCrc32(const std::uint8_t* bytes,std::size_t n) noexcept {
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<n;++i) {
        crc^=bytes[i];
        for(unsigned bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));
    }
    return ~crc;
}
namespace {
using Kind=RosterSaveErrorKind;
using Bytes=std::vector<std::uint8_t>;
using Tag=std::array<char,4>;
constexpr Tag team_tag{'T','E','A','M'},free_tag{'F','R','E','E'};
constexpr std::array<std::uint8_t,8> magic{'N','9','7','R','O','S','T',0};
[[noreturn]] void fail(Kind k,const char* why) { throw RosterSaveError(k,why); }
void number(Bytes& out,std::uint64_t value,unsigned n) {
    for(unsigned i=0;i<n;++i) { out.push_back(static_cast<std::uint8_t>(value)); value>>=8; }
}
std::uint64_t number(const Bytes& b,std::size_t at,unsigned n) {
    if(at>b.size() || n>b.size()-at) fail(Kind::Corrupt,"Truncated roster save field");
    std::uint64_t value=0;
    for(unsigned i=0;i<n;++i) value|=std::uint64_t(b[at+i])<<(i*8);
    return value;
}
void validTag(const Tag& tag) {
    for(char c:tag) if(!((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_'))
        fail(Kind::Corrupt,"Invalid roster section tag");
}
void validate(const RosterSlots& slots,const RosterSlots& base) {
    // Fixed scratch space: no hash map/allocation proportional to file data.
    auto sorted=slots, original=base;
    std::sort(sorted.begin(),sorted.end()); std::sort(original.begin(),original.end());
    if(sorted!=original) fail(Kind::InvalidRoster,"Roster save changes the supported base population");
    std::uint16_t previous=UINT16_MAX;
    for(auto id:sorted) {
        if(id==UINT16_MAX) break;
        if(id>=0x8000 || id==previous) fail(Kind::InvalidRoster,"Unknown, duplicate or unsupported roster ID");
        previous=id;
    }
}
void appendList(Bytes& out,unsigned team,const RosterSlots& slots) {
    const unsigned count=team==29 ? 100 : 15;
    number(out,team,2); number(out,count,2);
    for(unsigned i=0;i<count;++i) {
        const auto id=slots[team*15+i];
        number(out,id==UINT16_MAX ? UINT32_MAX : id,4);
    }
}
// Borrow payloads during encode; opaque optional data is not copied into a
// second staging vector. Known payloads live until the final output is built.
struct Section { Tag tag; std::uint16_t version,flags; const Bytes* bytes; };
}

std::vector<std::uint8_t> encodeRosterSave(const RosterSaveDocument& doc,
        const RosterSlots& base,const RosterBaseIdentity& identity) {
    validate(doc.slots,base);
    if(!doc.generation) fail(Kind::Corrupt,"Durable roster generations start at one");
    std::vector<Section> sections;
    Bytes teams,free;
    for(unsigned team=0;team<29;++team)
        if(!std::equal(doc.slots.begin()+team*15,doc.slots.begin()+(team+1)*15,base.begin()+team*15))
            appendList(teams,team,doc.slots);
    if(!teams.empty()) sections.push_back({team_tag,1,1,&teams});
    if(!std::equal(doc.slots.begin()+435,doc.slots.end(),base.begin()+435)) {
        appendList(free,29,doc.slots); sections.push_back({free_tag,1,1,&free});
    }
    if(doc.extensions.size()>kRosterSaveMaxSections-sections.size()) fail(Kind::Corrupt,"Too many roster sections");
    std::size_t size=68;
    for(const auto& s:sections) size+=16+s.bytes->size();
    for(const auto& ext:doc.extensions) {
        validTag(ext.tag);
        if(ext.tag==team_tag || ext.tag==free_tag) fail(Kind::Corrupt,"Reserved roster extension tag");
        if(size>kRosterSaveMaxBytes-16 || ext.payload.size()>kRosterSaveMaxBytes-size-16)
            fail(Kind::Corrupt,"Roster save exceeds size limit");
        size+=16+ext.payload.size();
        sections.push_back({ext.tag,ext.version,0,&ext.payload});
    }
    std::sort(sections.begin(),sections.end(),[](const auto& a,const auto& b){ return a.tag<b.tag; });
    for(std::size_t i=1;i<sections.size();++i)
        if(sections[i-1].tag==sections[i].tag) fail(Kind::Corrupt,"Duplicate roster section");
    Bytes out; out.reserve(size);
    out.insert(out.end(),magic.begin(),magic.end());
    number(out,1,2); number(out,doc.minor_version,2); number(out,64,2); number(out,0,2);
    number(out,size,4); number(out,sections.size(),2); number(out,0,2); number(out,doc.generation,8);
    out.insert(out.end(),identity.begin(),identity.end());
    for(const auto& s:sections) {
        out.insert(out.end(),s.tag.begin(),s.tag.end()); number(out,s.version,2); number(out,s.flags,2);
        number(out,s.bytes->size(),4); number(out,rosterSaveCrc32(s.bytes->data(),s.bytes->size()),4);
        out.insert(out.end(),s.bytes->begin(),s.bytes->end());
    }
    number(out,rosterSaveCrc32(out.data(),out.size()),4);
    return out;
}

RosterSaveDocument decodeRosterSave(const Bytes& b,const RosterSlots& base,const RosterBaseIdentity& identity) {
    if(b.size()<68 || b.size()>kRosterSaveMaxBytes || !std::equal(magic.begin(),magic.end(),b.begin()))
        fail(Kind::Corrupt,"Missing or invalid roster save header");
    // Unsupported is NOT corruption: a storage adapter must not use backup
    // fallback to silently downgrade/overwrite a newer required format.
    if(number(b,8,2)!=1) fail(Kind::Unsupported,"Unsupported roster save major version");
    if(number(b,12,2)!=64 || number(b,14,2)!=0 || number(b,22,2)!=0)
        fail(Kind::Unsupported,"Unsupported roster save header extension");
    const auto count=number(b,20,2);
    if(number(b,16,4)!=b.size() || count>kRosterSaveMaxSections ||
       number(b,b.size()-4,4)!=rosterSaveCrc32(b.data(),b.size()-4))
        fail(Kind::Corrupt,"Roster save length/count/checksum mismatch");
    if(!std::equal(identity.begin(),identity.end(),b.begin()+32)) fail(Kind::WrongBase,"Roster save belongs to a different base catalogue");
    RosterSaveDocument doc; doc.slots=base;
    doc.minor_version=static_cast<std::uint16_t>(number(b,10,2)); doc.generation=number(b,24,8);
    if(!doc.generation) fail(Kind::Corrupt,"Invalid zero roster generation");
    std::size_t at=64; Tag previous{};
    for(unsigned section=0;section<count;++section) {
        if(at>b.size()-4 || b.size()-4-at<16) fail(Kind::Corrupt,"Truncated roster section header");
        Tag tag; std::copy_n(b.begin()+at,4,tag.begin()); validTag(tag);
        if(section && !(previous<tag)) fail(Kind::Corrupt,"Duplicate or noncanonical roster section order");
        previous=tag;
        const auto version=number(b,at+4,2),flags=number(b,at+6,2),length=number(b,at+8,4),crc=number(b,at+12,4);
        at+=16;
        if(length>b.size()-4-at) fail(Kind::Corrupt,"Roster section outside file");
        const auto end=at+static_cast<std::size_t>(length);
        if(flags&~1u) fail(Kind::Unsupported,"Unsupported roster section flags");
        if(crc!=rosterSaveCrc32(b.data()+at,static_cast<std::size_t>(length))) fail(Kind::Corrupt,"Roster section checksum mismatch");
        if(tag==team_tag || tag==free_tag) {
            if(version!=1 || flags!=1) fail(Kind::Unsupported,"Unsupported required roster list section");
            if(tag==team_tag ? (!length || length%64 || length>29*64) : length!=404)
                fail(Kind::Corrupt,"Invalid roster list section extent");
            int prior=-1;
            while(at<end) {
                const unsigned team=static_cast<unsigned>(number(b,at,2)),slots=static_cast<unsigned>(number(b,at+2,2));
                at+=4;
                if(tag==team_tag ? (team>=29 || static_cast<int>(team)<=prior || slots!=15) : (team!=29 || slots!=100))
                    fail(Kind::Corrupt,"Invalid/duplicate roster list descriptor");
                prior=static_cast<int>(team);
                for(unsigned i=0;i<slots;++i) {
                    const auto id=number(b,at,4); at+=4;
                    if(id!=UINT32_MAX && id>=0x8000) fail(Kind::InvalidRoster,"Unsupported wire player ID; refusing truncation");
                    doc.slots[team*15+i]=id==UINT32_MAX ? UINT16_MAX : static_cast<std::uint16_t>(id);
                }
            }
        } else {
            if(flags&1u) fail(Kind::Unsupported,"Unknown required roster section");
            doc.extensions.push_back({tag,static_cast<std::uint16_t>(version),Bytes(b.begin()+at,b.begin()+end)});
            at=end;
        }
    }
    if(at!=b.size()-4) fail(Kind::Corrupt,"Trailing/unclaimed roster save bytes");
    validate(doc.slots,base);
    return doc;
}
}
