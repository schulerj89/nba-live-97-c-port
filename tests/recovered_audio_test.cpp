#include "recovered_audio.hpp"
#include "psx_adpcm.hpp"
#include "cool_fact_index.hpp"
#include "recovered/frontend_audio.h"

#include <algorithm>
#include <array>
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
        for (const auto* name : {"fixture.vh","fixture.vb","zcursor_pitch.bin","raw.wav","pitched.wav","default.wav",
                                "fact.idx","fact.big","fact.wav"})
            std::filesystem::remove(path/name,ignored);
        std::filesystem::remove(path,ignored);
    }
};

std::uint32_t referencePitch(const Bytes& table,int cents) {
    require(table.size()==256,"reference pitch table must contain 256 byte entries");
    // 72048: signed division truncates toward zero, then tests the INDEX's
    // sign. In particular -1..-4 cents still use nonnegative entry zero.
    const int index=static_cast<int>(std::int64_t(cents)*0x369d/65536);
    require(index>=-255 && index<=255,"reference pitch outside audited octave");
    return (256u+table.at(index<0 ? 256+index:index))*(index<0 ? 4u:8u);
}

bool sameInfo(const nba97::RecoveredClipInfo& a,const nba97::RecoveredClipInfo& b) {
    return a.record==b.record && a.sample_rate==b.sample_rate && a.sample_count==b.sample_count &&
        a.compressed_bytes==b.compressed_bytes && a.source==b.source &&
        a.program_volume==b.program_volume && a.tone_volume==b.tone_volume &&
        a.playback_volume==b.playback_volume && a.pitch_cents==b.pitch_cents &&
        a.root_note==b.root_note && a.requested_note==b.requested_note &&
        a.rendered_sample_count==b.rendered_sample_count && a.playback_suppressed==b.playback_suppressed &&
        a.authored_volume==b.authored_volume && a.effective_volume==b.effective_volume &&
        a.pitch_register==b.pitch_register && a.left_volume==b.left_volume && a.right_volume==b.right_volume;
}

Bytes cursorHeader(int cents=100,unsigned program_volume=96,unsigned tone_volume=80) {
    // Invented supported source-domain BNKl/PATl/tone/TMxl with a constant
    // envelope, full key/velocity ranges, centered pan, and one ADPCM block.
    Bytes header(136,0);
    tag(header,0,"BNKl");header[4]=1;put(header,6,2,2);put(header,12,4);
    constexpr unsigned program=16,tone=32,mapping=72,envelope=128;
    tag(header,program,"PATl");header[program+4]=1;header[program+6]=1;header[program+7]=1;
    put(header,program+8,static_cast<std::uint16_t>(cents),2);
    header[program+10]=64;header[program+11]=static_cast<std::uint8_t>(program_volume);put(header,program+12,4);
    header[tone+1]=127;header[tone+3]=127;put(header,tone+4,0xffffffff);
    header[tone+9]=60;header[tone+10]=1;header[tone+11]=255;header[tone+12]=64;
    header[tone+15]=1;header[tone+16]=64;header[tone+18]=static_cast<std::uint8_t>(tone_volume);
    put(header,tone+36,envelope-(tone+36));put(header,envelope,0xffffffff);put(header,envelope+4,127);
    tag(header,mapping,"TMxl");header[mapping+5]=16;header[mapping+6]=1;header[mapping+7]=6;
    put(header,mapping+10,22050,2);put(header,mapping+12,2048,2);put(header,mapping+16,28);
    put(header,mapping+20,0xffffffff);put(header,mapping+24,0xffffffff);
    put(header,mapping+36,16);put(header,mapping+44,0xffffffff);put(header,mapping+48,0xffffffff);
    return header;
}

void verifyClip(nba97::RecoveredAudioPlayer& player, const std::filesystem::path& header,
                const std::filesystem::path& body, unsigned id, const Temp& temp) {
    const auto bank=read(header), samples=read(body), pitch_table=read(header.parent_path()/"zcursor_pitch.bin");
    const auto pointer=8+id*4;
    const auto program=pointer+get(bank,pointer);
    const auto tone=program+12+get(bank,program+12);
    const auto mapping=tone+40;
    const auto count=get(bank,mapping+16), offset=get(bank,mapping+28), bytes=get(bank,mapping+36);
    require(offset<=samples.size() && bytes<=samples.size()-offset,"reference sample range invalid");
    const auto decoded=nba97::decodePsxAdpcmMono(samples.data()+offset,bytes,count);
    require(!decoded.empty(),"empty reference decode");
    require(std::any_of(decoded.begin(),decoded.end(),[](auto x){return x!=0;}),"fixture contains no signal");
    const int cents=static_cast<std::int16_t>(get(bank,program+8,2))+
        static_cast<std::int16_t>(get(bank,tone+20,2))-100*(int(bank.at(tone+9))-60);
    const auto pitch=referencePitch(pitch_table,cents);
    const auto authored=unsigned(bank.at(program+11))*bank.at(tone+18)/127;
    std::uint32_t pitched_count=0;
    for (unsigned setting=0;setting<=11;++setting) {
        const unsigned volume=std::min(setting*12u,127u);
        const unsigned effective=authored*volume/127;
        const auto raw=player.exportCursorSoundRaw(header,body,id,temp.path/"raw.wav",std::uint8_t(setting));
        const auto wav=read(temp.path/"raw.wav");
        require(raw.playback_volume==volume && raw.rendered_sample_count==count,"raw volume/count incorrect");
        require(raw.program_volume==bank.at(program+11) && raw.tone_volume==bank.at(tone+18) &&
                raw.authored_volume==authored && raw.effective_volume==effective &&
                raw.left_volume==effective*129 && raw.right_volume==effective*129,
                "cursor source gain/pan scalar metadata differs");
        require(raw.pitch_cents==cents && raw.pitch_register==pitch && raw.requested_note==60 &&
                raw.root_note==bank.at(tone+9),"cursor quantized pitch metadata differs");
        require(!raw.playback_suppressed && !player.isPlaying(),"zero-level export must decode without opening a device");
        require(wav.size()==44+decoded.size()*2,"WAV byte count incorrect");
        require(get(wav,24)==get(bank,mapping+10,2),"WAV rate changed with volume");
        for(std::size_t i=0;i<decoded.size();++i) {
            // Independent integer oracle; check every signed sample, not just RMS.
            const auto expected=static_cast<std::int16_t>(
                std::int64_t(decoded[i])*effective/127);
            require(static_cast<std::int16_t>(get(wav,44+i*2,2))==expected,"raw PCM gain differs");
        }
        const auto pitched=player.exportCursorSound(header,body,id,temp.path/"pitched.wav",std::uint8_t(setting));
        require(pitched.playback_volume==volume && pitched.pitch_cents==raw.pitch_cents,
                "gain altered authored pitch metadata");
        require(pitched.pitch_register==pitch && pitched.authored_volume==authored &&
                pitched.effective_volume==effective && pitched.left_volume==effective*129 &&
                pitched.right_volume==effective*129,"pitched export lost recovered scalar metadata");
        if (!setting) pitched_count=pitched.rendered_sample_count;
        require(pitched.rendered_sample_count==pitched_count,"gain altered pitched duration");
        require(pitched_count==std::max(1u,static_cast<unsigned>((std::uint64_t(count)*2048+pitch-1)/pitch)),
                "pitch duration does not use quantized SPU register ratio");
        const auto pitched_wav=read(temp.path/"pitched.wav");
        require(pitched_wav.size()==44+pitched_count*2,"pitched WAV count differs");
        // Verify gain precedes interpolation, including negative-sample truncation.
        const double ratio=double(pitch)/2048;
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

void rejectedCursors(nba97::RecoveredAudioPlayer& player,const Temp& temp) {
    const auto valid_header=read(temp.path/"fixture.vh"),valid_body=read(temp.path/"fixture.vb");
    const auto valid_pitch=read(temp.path/"zcursor_pitch.bin");
    const auto before=player.info();const auto old_output=read(temp.path/"raw.wav");
    unsigned callbacks=0;
    struct UnexpectedAcceptance {};
    // A rejected fixture must never reach this callback. Throwing also prevents
    // device submission if a parser regression unexpectedly accepts a fixture.
    const auto accepted=[&] {++callbacks;throw UnexpectedAcceptance{};};
    auto reject=[&](unsigned id=1) {
        bool rejected=false;
        try {(void)player.playCursorSound(temp.path/"fixture.vh",temp.path/"fixture.vb",id,9,accepted);}
        catch(const std::runtime_error&) {rejected=true;}
        catch(const UnexpectedAcceptance&) {}
        require(rejected && callbacks==0 && sameInfo(player.info(),before) && !player.isPlaying(),
                "invalid cursor source accepted, called back, or changed playback metadata");
        rejected=false;
        try {(void)player.exportCursorSound(temp.path/"fixture.vh",temp.path/"fixture.vb",id,temp.path/"raw.wav");}
        catch(const std::runtime_error&) {rejected=true;}
        require(rejected && sameInfo(player.info(),before) && read(temp.path/"raw.wav")==old_output,
                "invalid cursor export changed metadata/output or was accepted");
    };
    reject(0);reject(2);reject(0xffffffff); // null and out-of-range BNKl entries
    for(unsigned length : {0u,7u,15u,31u,123u,135u}) {
        write(temp.path/"fixture.vh",Bytes(valid_header.begin(),valid_header.begin()+length));reject();
    }
    write(temp.path/"fixture.vh",valid_header);
    for(unsigned length : {0u,15u}) {write(temp.path/"fixture.vb",Bytes(valid_body.begin(),valid_body.begin()+length));reject();}
    auto bad_body=valid_body;bad_body[0]=0xf4;write(temp.path/"fixture.vb",bad_body);reject();
    write(temp.path/"fixture.vb",valid_body);
    for(unsigned length : {0u,1u,255u,257u}) {
        auto table=valid_pitch;table.resize(length);write(temp.path/"zcursor_pitch.bin",table);reject();
    }
    std::filesystem::remove(temp.path/"zcursor_pitch.bin");reject();
    write(temp.path/"zcursor_pitch.bin",valid_pitch);
    // Each mutation changes one supported source field independently. Include
    // source pointer/range checks and scalar limits, not only magic strings.
    for(const auto& mutation : std::vector<std::array<std::uint32_t,3>>{
        {0,0,4},{6,0xffff,2},{12,0xffffffff,4},{16,0,4},
        {20,2,1},{21,1,1},{22,2,1},{23,2,1},{26,0,1},{27,128,1},{28,0,4},
        {33,1,1},{35,1,1},{36,0,4},{48,0,1},{49,1,1},{50,128,1},{51,1,1},
        {54,1,2},{56,1,4},{60,1,4},{64,1,4},{68,0,4},
        {72,0,4},{77,8,1},{78,2,1},{79,0,1},{82,0,2},{84,0,2},
        {88,29,4},{100,1,4},{108,15,4},{128,0,4},{132,128,4}}) {
        auto header=valid_header;put(header,mutation[0],mutation[1],mutation[2]);
        write(temp.path/"fixture.vh",header);reject();
    }
    for(int cents : {-1201,1201}) {
        auto header=valid_header;put(header,24,static_cast<std::uint16_t>(cents),2);
        write(temp.path/"fixture.vh",header);reject();
    }
    write(temp.path/"fixture.vh",valid_header);
    const auto muted=player.playCursorSound(temp.path/"absent.vh",temp.path/"absent.vb",999,0,accepted);
    require(muted.playback_suppressed && !muted.playback_volume && !muted.rendered_sample_count && !callbacks &&
            sameInfo(player.info(),before) && !player.isPlaying(),"mute must bypass files, callback and device without changing info");
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
        // Invented cursor bank plus a deliberately simple, non-retail byte
        // table. No copyrighted assets or audio device are needed for CTest.
        Bytes body(16,0x97),pitch_table(256);
        for(unsigned i=0;i<256;++i)pitch_table[i]=static_cast<std::uint8_t>(i);
        body[0]=4; body[1]=1;
        write(temp.path/"fixture.vb",body);write(temp.path/"zcursor_pitch.bin",pitch_table);
        nba97::RecoveredAudioPlayer player;
        for(int cents : {-1200,-100,-5,-4,0,4,5,100,1200}) {
            write(temp.path/"fixture.vh",cursorHeader(cents));
            verifyClip(player,temp.path/"fixture.vh",temp.path/"fixture.vb",1,temp);
        }
        for(const auto gains : {std::array<unsigned,2>{41,44},{41,66},{127,127},{0,127},{127,0}}) {
            write(temp.path/"fixture.vh",cursorHeader(100,gains[0],gains[1]));
            verifyClip(player,temp.path/"fixture.vh",temp.path/"fixture.vb",1,temp);
        }
        std::cout<<"AUDIO PASS synthetic_168_cursor_levels_double_floor_gain_and_quantized_pitch\n";
        write(temp.path/"fixture.vh",cursorHeader());
        rejectedCursors(player,temp);
        std::cout<<"AUDIO PASS cursor_source_rejections_atomic_info_output_and_mute_callback_guard\n";
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
