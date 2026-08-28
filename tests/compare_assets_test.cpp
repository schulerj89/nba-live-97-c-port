#include "compare_assets.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
struct Fixture {
    std::filesystem::path directory,path;
    Fixture() {
        const auto seed=std::chrono::steady_clock::now().time_since_epoch().count();
        for(int i=0;i<100;++i) {
            const auto p=std::filesystem::temp_directory_path()/("nba97-compare-pack-"+std::to_string(seed)+"-"+std::to_string(i));
            if(std::filesystem::create_directory(p)) { directory=p; break; }
        }
        check(!directory.empty(),"unique fixture directory"); path=directory/"synthetic.n97ui";
    }
    void write(const std::vector<unsigned char>& b) {
        std::ofstream out(path,std::ios::binary|std::ios::trunc);
        out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
        check(bool(out),"fixture write");
    }
    ~Fixture() { std::error_code ignored; std::filesystem::remove(path,ignored); std::filesystem::remove(directory,ignored); }
};
std::vector<unsigned char> pack() {
    std::vector<unsigned char> b{'N','9','7','C',1,0,13,0};
    auto half=[&](unsigned v) { b.push_back(static_cast<unsigned char>(v)); b.push_back(static_cast<unsigned char>(v>>8)); };
    auto text=[&](const std::string& s) { half(static_cast<unsigned>(s.size())); b.insert(b.end(),s.begin(),s.end()); };
    for(int i=0;i<13;++i) text("Synthetic");
    const std::vector<std::vector<int>> fields{
        {58,59,60,53,54,56,57,55,64,61,62,63,65,66},
        {-1,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29},
        {22,23,6,32,8,34,0,43,1,44,2,45,15,16,17,37,18,38,19,39,20,40,21,25}};
    for(const auto& family:fields) {
        half(static_cast<unsigned>(family.size()));
        for(int field:family) { half(static_cast<unsigned>(field)); text("Synthetic"); }
    }
    return b;
}
}
int main(int argc,char** argv) {
    try {
        check(argc<=2,"usage: nba97_compare_assets_tests [private compare.n97ui]");
        Fixture fixture; const auto valid=pack(); fixture.write(valid);
        nba97::CompareAssets assets(fixture.path);
        check(assets.text(12)=="Synthetic" && assets.label(3,23)=="Synthetic","loaded labels");
        nba97::RosterDatabase db; nba97::PlayerRecord player; player.nickname="Synthetic nickname";
        player.ratings.fill(50); player.ratings[0]=17; player.season_1995_96.valid=true;
        player.season_1995_96.games_played=12; player.playoffs_1995_96.valid=true; player.playoffs_1995_96.games_played=3;
        check(assets.value(db,player,0,0)==player.nickname && assets.value(db,player,1,1)=="17" &&
              assets.value(db,player,2,0)=="12" && assets.value(db,player,3,0)=="3","field/period formatting");
        std::cout<<"COMPARE-ASSET PASS synthetic_roundtrip_field_period_mapping\n";
        auto refused=[&](std::vector<unsigned char> b) {
            fixture.write(b); bool rejected=false;
            try { nba97::CompareAssets invalid(fixture.path); } catch(const std::runtime_error&) { rejected=true; }
            check(rejected,"malformed pack accepted");
        };
        auto bad=valid; bad[4]=2; refused(bad); bad=valid; bad[6]=12; refused(bad);
        bad=valid; bad.push_back(0); refused(bad); refused(std::vector<unsigned char>(8193,0));
        for(auto n:{0u,7u,20u,static_cast<unsigned>(valid.size()-1)}) refused({valid.begin(),valid.begin()+n});
        bad=valid; bad[8]=129; refused(bad); bad=valid; bad[10]=31; refused(bad);
        bad=valid; bad[8+13*11]=15; refused(bad); // family extent
        bad=valid; bad[8+13*11+2]=67; refused(bad); // field ID
        std::cout<<"COMPARE-ASSET PASS malformed_versions_bounds_fields_controls\n";
        bool out_of_range=false;
        try { assets.value(db,player,4,0); } catch(const std::runtime_error&) { out_of_range=true; }
        check(out_of_range,"special layer silently accepted");
        out_of_range=false; try { assets.label(0,14); } catch(const std::out_of_range&) { out_of_range=true; }
        check(out_of_range,"descriptor out-of-range accepted");
        std::cout<<"COMPARE-ASSET PASS unsupported_layer_and_row_guards\n";
        if(argc==2) {
            nba97::CompareAssets local(argv[1]);
            for(unsigned layer=0;layer<4;++layer) for(unsigned i=0;i<(layer==0 ? 14u : layer==1 ? 17u : 24u);++i)
                check(!local.label(layer,i).empty(),"missing private label");
            check(local.value(db,player,2,0)=="12","private field mapping differs");
            std::cout<<"COMPARE-ASSET PASS private_original_pack\n";
        }
        return 0;
    } catch(const std::exception& e) { std::cerr<<"COMPARE-ASSET FAIL "<<e.what()<<'\n'; return 1; }
}
