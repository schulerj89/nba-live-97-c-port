#include "trade_assets.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool ok){if(!ok)throw std::runtime_error("Trade pack test failed");}
std::vector<uint8_t> fixture(bool sign=false) {
    std::vector<uint8_t> out{'N','9','7','T',1,0,uint8_t(sign?25:21),0};
    auto word=[&](uint32_t v){for(int i=0;i<4;++i)out.push_back(uint8_t(v>>(8*i)));};
    auto record=[&](uint32_t a,const std::vector<uint8_t>& b){word(a);word(uint32_t(b.size()));out.insert(out.end(),b.begin(),b.end());};
    for(auto a:{0x8002655Cu,0x80026574u,0x80026588u,0x8002659Cu,0x80026508u,0x8002650Cu,
                0x80026510u,0x80026514u,0x80026518u,0x8002651Cu,0x8002502Cu,0x80024E60u})
        record(a,{'t','e','s','t',0});
    record(0x800264EC,{'V','I','E','W',0});record(0x800264F8,{'C','O','M','P','A','R','E',0});
    for(auto a:{0x800AEBB2u,0x800AECBEu,0x800AFC22u,0x800AF4F8u,0x800AEE88u,0x800AEEF6u}) {
        const uint8_t choices=(a==0x800AF4F8 || a==0x800AEE88 || a==0x800AEEF6)?2:0;
        std::vector<uint8_t> b{121,0,70,0,14,1,100,1,1,choices,1,'%','s',0};
        for(int i=0;i<choices;++i)b.insert(b.end(),{1,'o','k',0});record(a,b);
    }
    if(sign) {
        record(0x8009D83A,{'f','r','e','e',0});
        for(auto a:{0x800AED20u,0x800AEC72u,0x800AED88u})
            record(a,{121,0,70,0,14,1,100,1,1,0,1,'%','s',0});
    }
    record(0x800265AC,std::vector<uint8_t>(25,0));return out;
}
void reject(const std::vector<uint8_t>& b){bool failed=false;try{nba97::TradeAssets p(b);}catch(const std::runtime_error&){failed=true;}check(failed);}
}
int main(){try{
    const auto valid=fixture();nba97::TradeAssets assets(valid);
    check(assets.rect(0x800AEE88).width==270 && assets.emptyNotice(false).lines.size()==2);
    check(assets.emptyNotice(false).lines[0].encoded=="VIEW" && assets.emptyNotice(true).lines[0].encoded=="COMPARE");
    bool missing=false;try{assets.notice(0x800AFC22);}catch(const std::runtime_error&){missing=true;}check(missing);
    std::cout<<"TRADE-ASSET PASS original_route_contracts_with_synthetic_bytes\n";
    for(size_t n=0;n<valid.size();++n)reject({valid.begin(),valid.begin()+n});
    auto b=valid;b.push_back(0);reject(b);b=valid;b[4]=2;reject(b);b=valid;b[6]=18;reject(b);
    b=valid;b.back()=5;reject(b);b=valid;b[16]=31;reject(b);
    b=valid;std::copy_n(b.begin()+8,4,b.begin()+21);reject(b); // duplicate text route
    std::cout<<"TRADE-ASSET PASS truncation_versions_controls_preferences_duplicates\n";
    const auto sign=fixture(true);nba97::TradeAssets signAssets(sign);
    check(signAssets.freeAgentName()=="free");
    for(auto a:{0x800AED20u,0x800AEC72u,0x800AED88u}) {
        const auto notice=signAssets.notice(a,"receiver");
        check(notice.state==14&&notice.lines[0].encoded=="receiver");
    }
    for(size_t n=0;n<sign.size();++n)reject({sign.begin(),sign.begin()+n});
    b=sign;b[6]=21;reject(b);b=valid;b[6]=25;reject(b);
    std::cout<<"SIGN-ASSET PASS required_routes_substitution_truncation_and_screen_counts\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
