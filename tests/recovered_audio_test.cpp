#include "recovered_audio.hpp"
#include "psx_adpcm.hpp"
#include "cool_fact_index.hpp"
#include "recovered/frontend_audio.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
using Bytes = std::vector<std::uint8_t>;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
Bytes read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    require(bool(in), "cannot read test audio");
    return {(std::istreambuf_iterator<char>(in)), {}};
}
void write(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    require(bool(out), "cannot write test fixture");
}
void put(Bytes& bytes, std::size_t offset, std::uint32_t value, unsigned size=4) {
    for (unsigned i=0;i<size;++i) bytes.at(offset+i)=static_cast<std::uint8_t>(value>>(8*i));
}
std::uint32_t get(const Bytes& bytes, std::size_t offset, unsigned size=4) {
    std::uint32_t value=0;
    for (unsigned i=0;i<size;++i) value|=std::uint32_t(bytes.at(offset+i))<<(8*i);
    return value;
}
void tag(Bytes& bytes, std::size_t offset, const char* text) {
    std::copy_n(text,4,bytes.begin()+offset);
}
struct Temp {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("nba97-audio-gain-"+std::to_string(GetCurrentProcessId()));
    Temp() { require(std::filesystem::create_directory(path), "test temp directory already exists"); }
    ~Temp() {
        std::error_code ignored;
        // Only files created inside this newly owned test directory.
        for (const auto* name : {"fixture.vh","fixture.vb","raw.wav","pitched.wav","default.wav",
                                "fact.idx","fact.big","fact.wav"})
            std::filesystem::remove(path/name,ignored);
        std::filesystem::remove(path,ignored);
    }
};

void verifyClip(nba97::RecoveredAudioPlayer& player, const std::filesystem::path& header,
                const std::filesystem::path& body, unsigned id, const Temp& temp) {
    const auto bank=read(header), samples=read(body);
    const auto pointer=8+id*4;
    const auto program=pointer+get(bank,pointer);
    const auto tone=program+12+get(bank,program+12);
    const auto mapping=tone+40;
    const auto count=get(bank,mapping+16), offset=get(bank,mapping+28), bytes=get(bank,mapping+36);
    require(offset<=samples.size() && bytes<=samples.size()-offset,"reference sample range invalid");
    const auto decoded=nba97::decodePsxAdpcmMono(samples.data()+offset,bytes,count);
    require(!decoded.empty(),"empty reference decode");
    require(std::any_of(decoded.begin(),decoded.end(),[](auto x){return x!=0;}),"fixture contains no signal");
    std::uint32_t pitched_count=0;
    for (unsigned setting=0;setting<=11;++setting) {
        const unsigned volume=std::min(setting*12u,127u);
        const auto raw=player.exportCursorSoundRaw(header,body,id,temp.path/"raw.wav",std::uint8_t(setting));
        const auto wav=read(temp.path/"raw.wav");
        require(raw.playback_volume==volume && raw.rendered_sample_count==count,"raw volume/count incorrect");
        require(wav.size()==44+decoded.size()*2,"WAV byte count incorrect");
        require(get(wav,24)==get(bank,mapping+10,2),"WAV rate changed with volume");
        for(std::size_t i=0;i<decoded.size();++i) {
            // Independent integer oracle; check every signed sample, not just RMS.
            const auto expected=static_cast<std::int16_t>(
                std::int64_t(decoded[i])*bank.at(program+11)*bank.at(tone+18)*volume/(127LL*127*127));
            require(static_cast<std::int16_t>(get(wav,44+i*2,2))==expected,"raw PCM gain differs");
        }
        const auto pitched=player.exportCursorSound(header,body,id,temp.path/"pitched.wav",std::uint8_t(setting));
        require(pitched.playback_volume==volume && pitched.pitch_cents==raw.pitch_cents,
                "gain altered authored pitch metadata");
        if (!setting) pitched_count=pitched.rendered_sample_count;
        require(pitched.rendered_sample_count==pitched_count,"gain altered pitched duration");
        const auto pitched_wav=read(temp.path/"pitched.wav");
        require(pitched_wav.size()==44+pitched_count*2,"pitched WAV count differs");
        // Verify gain precedes interpolation, including negative-sample truncation.
        const double ratio=std::pow(2.0,raw.pitch_cents/1200.0);
        for(std::size_t i=0;i<pitched_count;++i) {
            const double position=i*ratio;
            const auto lo=std::min(static_cast<std::size_t>(position),decoded.size()-1);
            const auto hi=std::min(lo+1,decoded.size()-1);
            const auto a=static_cast<std::int16_t>(get(wav,44+lo*2,2));
            const auto b=static_cast<std::int16_t>(get(wav,44+hi*2,2));
            const auto expected=static_cast<std::int16_t>(std::clamp(std::lround(a+(b-a)*(position-lo)), -32768L,32767L));
            require(static_cast<std::int16_t>(get(pitched_wav,44+i*2,2))==expected,"pitched PCM gain order differs");
        }
        if(setting==9) {
            player.exportCursorSound(header,body,id,temp.path/"default.wav");
            require(read(temp.path/"default.wav")==pitched_wav,"default-9 export changed");
        }
    }
}

void factFixture(nba97::RecoveredAudioPlayer& player,const Temp& temp) {
    Bytes index(4+11*8,0),archive;
    put(index,0,10); // Ten logical records PLUS physical fallback record0.
    for(unsigned p=0;p<2;++p) {
        const unsigned frames=p+1,physical=p*5+1;
        Bytes clip(0x74+frames*16+2,0);
        tag(clip,0,"PATl");tag(clip,0x38,"TMxl");
        clip[7]=1;clip[11]=p ? 127 : 96;put(clip,12,4);clip[25]=60;clip[34]=p ? 127 : 80;
        clip[0x3d]=16;clip[0x3e]=1;clip[0x3f]=6;
        put(clip,0x42,16000,2);put(clip,0x48,frames*28);
        for(unsigned f=0;f<frames;++f) {
            const auto at=0x74+f*16;clip[at]=4;clip[at+1]=f+1==frames ? 1 : 0;
            std::fill(clip.begin()+at+2,clip.begin()+at+16,p ? 0x99 : 0x77);
        }
        put(index,4+physical*8,static_cast<std::uint32_t>(clip.size()));
        put(index,8+physical*8,static_cast<std::uint32_t>(archive.size()));
        archive.insert(archive.end(),clip.begin(),clip.end());
    }
    write(temp.path/"fact.idx",index);write(temp.path/"fact.big",archive);
    for(unsigned p=0;p<2;++p) {
        const auto info=player.exportCoolFact(temp.path/"fact.idx",temp.path/"fact.big",
            static_cast<std::uint16_t>(p),0,temp.path/"fact.wav");
        require(info.record==p*5+1 && info.sample_count==(p+1)*28,"wrong physical speech record/player");
        const auto wav=read(temp.path/"fact.wav");
        require(wav.size()==44+(p+1)*56,"wrong player speech length");
        const auto first=static_cast<std::int16_t>(get(wav,44,2));
        require(p ? first<0 : first>0,"speech decoded adjacent player's payload");
    }
    bool absent=false;
    try {player.inspectCoolFact(temp.path/"fact.idx",temp.path/"fact.big",0,4);}
    catch(const std::runtime_error&) {absent=true;}
    require(absent,"player0 variant4 borrowed player1 variant0");
}

void verifyFactLevels(nba97::RecoveredAudioPlayer& player,const std::filesystem::path& index,
                      const std::filesystem::path& archive,unsigned id,unsigned variant,const Temp& temp) {
    const auto before=player.info();
    const auto decoded=player.prepareCoolFact(index,archive,static_cast<std::uint16_t>(id),variant);
    require(!player.isPlaying() && player.info().source==before.source &&
            player.info().record==before.record,"preparation changed playback metadata/device");
    require(decoded.info.pitch_cents==0,"gain fixture must have neutral pitch");
    for(unsigned setting=0;setting<12;++setting) {
        const auto info=player.exportCoolFactPlayback(index,archive,static_cast<std::uint16_t>(id),
            variant,temp.path/"fact.wav",static_cast<std::uint8_t>(setting));
        const auto wav=read(temp.path/"fact.wav");
        const auto gain=std::min(setting*15u,127u);
        require(info.playback_volume==gain && !info.playback_suppressed &&
                info.rendered_sample_count==decoded.pcm.size(),"speech gain/zero-volume lifecycle metadata");
        require(get(wav,24)==info.sample_rate && wav.size()==44+decoded.pcm.size()*2,
                "speech setting changed rate/duration");
        for(std::size_t i=0;i<decoded.pcm.size();++i) {
            const auto expected=static_cast<std::int16_t>(std::int64_t(decoded.pcm[i])*
                decoded.info.program_volume*decoded.info.tone_volume*gain/(127LL*127*127));
            require(static_cast<std::int16_t>(get(wav,44+i*2,2))==expected,"speech PCM gain differs");
        }
    }
}

void malformedFacts(nba97::RecoveredAudioPlayer& player,const Temp& temp) {
    const auto valid_index=read(temp.path/"fact.idx"),valid_archive=read(temp.path/"fact.big");
    const auto clip_bytes=get(valid_index,12);
    const auto before=player.info();
    for(unsigned cut=1;cut<clip_bytes;++cut) {
        auto index=valid_index;put(index,12,cut);
        write(temp.path/"fact.idx",index);
        bool rejected=false;
        try {(void)player.prepareCoolFact(temp.path/"fact.idx",temp.path/"fact.big",0,0);}
        catch(const std::runtime_error&) {rejected=true;}
        require(rejected && !player.isPlaying() && player.info().source==before.source,
                "truncated speech mutated state or was accepted");
    }
    write(temp.path/"fact.idx",valid_index);
    for(unsigned field : {7u,12u,11u,34u}) {
        auto archive=valid_archive;archive[field]=255;write(temp.path/"fact.big",archive);
        bool rejected=false;
        try {(void)player.prepareCoolFact(temp.path/"fact.idx",temp.path/"fact.big",0,0);}
        catch(const std::runtime_error&) {rejected=true;}
        require(rejected,"unsupported speech tone/gain was accepted");
    }
    write(temp.path/"fact.big",valid_archive);
}

void privateFacts(nba97::RecoveredAudioPlayer& player,const std::filesystem::path& root,const Temp& temp) {
    const auto raw=read(root/"Z1COOL.IDX");
    const nba97::CoolFactIndexView index(raw);
    require(index.count()==2465,"unexpected original logical speech count");
    std::ifstream headers(root/"Z1COOL.BIG",std::ios::binary);
    require(bool(headers),"missing original speech archive");
    unsigned populated=0;
    for(unsigned logical=0;logical<2465;++logical) {
        const auto entry=index.lookup(static_cast<std::uint16_t>(logical/5),logical%5);
        const auto at=12+logical*8; // Independently transcribed 315BC: +1, <<3, +4.
        require(entry.physical_record==logical+1 && entry.bytes==get(raw,at) && entry.offset==get(raw,at+4),
                "original speech index mapping mismatch");
        if(entry.bytes) {
            Bytes header(0x74);
            headers.seekg(entry.offset);headers.read(reinterpret_cast<char*>(header.data()),header.size());
            require(bool(headers) && header[7]==1 && get(header,12)==4 &&
                    header[11]==127 && header[34]==127 && header[25]==60 &&
                    get(header,8,2)==0 && get(header,36,2)==0,
                    "original speech program/tone gain or pitch baseline changed");
            ++populated;
        }
    }
    require(populated==1185,"original populated speech count changed");
    // All five first-player variants plus next player's first: exercise the
    // boundary that a one-record shift crosses. Never open an audio device.
    for(unsigned logical=0;logical<6;++logical) {
        const auto entry=index.lookup(static_cast<std::uint16_t>(logical/5),logical%5);
        const auto info=player.exportCoolFact(root/"Z1COOL.IDX",root/"Z1COOL.BIG",
            static_cast<std::uint16_t>(logical/5),logical%5,temp.path/"fact.wav");
        std::ifstream source(root/"Z1COOL.BIG",std::ios::binary);source.seekg(entry.offset);
        Bytes clip(entry.bytes);source.read(reinterpret_cast<char*>(clip.data()),clip.size());
        require(bool(source),"private speech range read failed");
        require(info.record==logical+1 && info.sample_count==get(clip,0x48),"private speech identity/length mismatch");
        const auto expected=nba97::decodePsxAdpcmMono(clip.data()+0x74,info.compressed_bytes,info.sample_count);
        const auto wav=read(temp.path/"fact.wav");
        require(wav.size()==44+expected.size()*2,"private speech WAV size mismatch");
        for(std::size_t i=0;i<expected.size();++i)
            require(static_cast<std::int16_t>(get(wav,44+i*2,2))==expected[i],"wrong source speech PCM payload");
        verifyFactLevels(player,root/"Z1COOL.IDX",root/"Z1COOL.BIG",logical/5,logical%5,temp);
    }
}
}

int main(int argc,char** argv) {
    try {
        Temp temp;
        for(unsigned i=0;i<256;++i)
            require(nba97_frontend_sfx_volume(std::uint8_t(i))==std::min(i*12u,127u),"volume clamp mismatch");
        std::cout<<"AUDIO PASS volume_256_inputs\n";
        for(unsigned i=0;i<256;++i)
            require(nba97_frontend_speech_volume(std::uint8_t(i))==std::min(i*15u,127u),"speech volume clamp");
        for(int stopped : {0,1}) for(unsigned feedback : {0u,1u,65535u})
            require(nba97_frontend_fact_stop_sound(stopped,static_cast<std::uint16_t>(feedback))==
                    (stopped && feedback ? 5 : 0),"speech stop feedback predicate");
        std::cout<<"AUDIO PASS speech_256_settings_and_stop_feedback\n";
        // Invented mono BNKl fixture: one bipolar ADPCM block, program/tone gain,
        // and +100 cents authored pitch. No game assets needed for CTest.
        Bytes header(128,0),body(16,0x97);
        tag(header,0,"BNKl"); header[4]=1; put(header,6,2,2);
        put(header,12,4); tag(header,16,"PATl"); header[23]=1;
        put(header,24,100,2); header[27]=96; put(header,28,4);
        header[41]=60; header[50]=80; tag(header,72,"TMxl");
        put(header,82,22050,2); put(header,88,28); put(header,100,0); put(header,108,16);
        body[0]=4; body[1]=1;
        write(temp.path/"fixture.vh",header); write(temp.path/"fixture.vb",body);
        nba97::RecoveredAudioPlayer player;
        verifyClip(player,temp.path/"fixture.vh",temp.path/"fixture.vb",1,temp);
        std::cout<<"AUDIO PASS synthetic_12_levels_exact_pcm_and_pitch\n";
        const auto previous=player.info();
        const auto muted=player.playCursorSound(temp.path/"absent.vh",temp.path/"absent.vb",999,0);
        require(muted.playback_suppressed && !muted.playback_volume && !muted.rendered_sample_count,
                "mute did not suppress playback");
        require(player.info().record==previous.record && player.info().playback_volume==previous.playback_volume &&
                player.info().source==previous.source && !player.isPlaying(),"mute changed player state");
        std::cout<<"AUDIO PASS mute_skips_bank_and_device\n";
        factFixture(player,temp);
        std::cout<<"AUDIO PASS synthetic_cool_fact_reserved_record_and_player_boundary\n";
        for(unsigned id=0;id<2;++id)
            verifyFactLevels(player,temp.path/"fact.idx",temp.path/"fact.big",id,0,temp);
        std::cout<<"AUDIO PASS synthetic_speech_24_gain_vectors_and_prepare_isolation\n";
        malformedFacts(player,temp);
        std::cout<<"AUDIO PASS synthetic_speech_truncations_and_invalid_tones\n";
        if(argc==2) {
            const std::filesystem::path root=argv[1];
            for(unsigned id=1;id<=12;++id) verifyClip(player,root/"ZCURSOR.VH",root/"ZCURSOR.VB",id,temp);
            std::cout<<"AUDIO PASS private_144_cue_levels_exact_pcm_and_pitch\n";
            privateFacts(player,root,temp);
            std::cout<<"AUDIO PASS private_2465_speech_mappings_and_six_clip_payloads\n";
            std::cout<<"AUDIO PASS private_speech_72_gain_vectors\n";
        }
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"AUDIO FAIL "<<error.what()<<'\n';
        return 1;
    }
}
