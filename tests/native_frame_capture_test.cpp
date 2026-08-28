#include "native_frame_capture.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace fs=std::filesystem;
using nba97::NativeFrameCapture;
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
std::string read(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);
    require(bool(in),"missing artifact");
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
template<class F> void rejects(F action) {bool threw=false;try {action();}catch(const std::exception&){threw=true;} require(threw,"expected refusal");}
int main() {
    fs::path root;
    try {
        // Exclusive fixture below the system temp directory, never live assets/saves.
        for(unsigned i=0;i<100;++i) {
            auto candidate=fs::temp_directory_path()/("nba97-record-test-"+std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(i));
            if(fs::create_directory(candidate)) {root=candidate;break;}
        }
        require(!root.empty(),"cannot create exclusive fixture");
        std::vector<std::uint8_t> frame(NativeFrameCapture::frame_bytes);
        for(std::size_t i=0;i<frame.size();i+=4) {frame[i]=1;frame[i+1]=2;frame[i+2]=3;frame[i+3]=255;}
        NativeFrameCapture::State state{};state[3]=-1;state[14]=8;
        {
            NativeFrameCapture capture(root,root/"normal");
            capture.input(50,256,70,123);
            require(capture.submit(frame,100,state),"first frame");
            capture.input(110,257,70,-1);
            require(capture.submit(frame,17300001,state),"identical frame must be retained");
            frame[2]=99;require(capture.submit(frame,60000000,state),"stalled presentation must retain actual time");
            capture.finish();capture.finish();
            require(capture.error().empty() && capture.submitted()==3 && !capture.accepting(),"finish status");
            require(!capture.submit(frame,61000000,state),"closed collector accepted data");
        }
        const auto ppm=read(root/"normal/00000.ppm");
        const std::string header="P6\n512 240\n255\n";
        require(ppm.size()==header.size()+512*240*3 && ppm.substr(0,header.size())==header,"lossless PPM bounds");
        require(ppm[header.size()]==3 && ppm[header.size()+1]==2 && ppm[header.size()+2]==1,"BGRA/RGB order");
        require(ppm==read(root/"normal/00001.ppm"),"unchanged presentations were altered/dropped");
        require(read(root/"normal/00002.ppm")[header.size()]==99,"changed frame lost");
        const auto timeline=read(root/"normal/frames.csv");
        require(timeline.find("1,17300001,")!=std::string::npos && timeline.find("2,60000000,")!=std::string::npos,"actual timestamp lost");
        require(read(root/"normal/inputs.csv").find("110,1,257,70,-1")!=std::string::npos,"input frame boundary");
        const auto report=read(root/"normal/recording.json");
        require(report.find("\"video_complete\":true")!=std::string::npos &&
                report.find("\"reference_ready\":false")!=std::string::npos &&
                report.find("\"audio_captured\":false")!=std::string::npos &&
                report.find("\"frame_rate\":null")!=std::string::npos,"false reference/fps/audio claim");
        std::cout<<"RECORD PASS pixel_order_identical_frames_variable_timing_input_boundary\n";
        rejects([&]{NativeFrameCapture c(root,root/"normal");});
        require(read(root/"normal/recording.json")==report,"existing recording overwritten");
        rejects([&]{NativeFrameCapture c(root,root);});
        rejects([&]{NativeFrameCapture c(root,root/".."/"escape");});
        std::cout<<"RECORD PASS exclusive_directory_and_private_root\n";
        {
            NativeFrameCapture c(root,root/"bad-clock");
            require(c.submit(frame,20,state),"clock setup");
            require(!c.submit(frame,20,state),"equal timestamp accepted");c.finish();
            require(!c.error().empty(),"clock error missing");
        }
        {
            NativeFrameCapture c(root,root/"bad-size");
            require(!c.submit({1,2,3},0,state),"bad extent accepted");c.finish();
            require(!c.error().empty(),"size error missing");
        }
        {
            NativeFrameCapture c(root,root/"bad-input");
            c.input(20,1,2,3);c.input(19,1,2,3);c.finish();require(!c.error().empty(),"backwards input accepted");
        }
        {
            NativeFrameCapture c(root,root/"writer-fail");
            fs::create_directory(root/"writer-fail/00000.ppm");
            require(c.submit(frame,1,state),"writer fail setup");c.finish();
            require(!c.error().empty(),"writer failure not propagated");
        }
        for(const char* name:{"bad-clock","bad-size","bad-input","writer-fail"})
            require(read(root/name/"recording.json").find("\"video_complete\":false")!=std::string::npos,"failed capture claimed complete");
        std::cout<<"RECORD PASS bad_clock_extent_input_and_disk_failure_are_incomplete\n";
        { NativeFrameCapture empty(root,root/"empty"); }
        require(read(root/"empty/recording.json").find("\"video_complete\":false")!=std::string::npos,"empty session passed");
        std::cout<<"RECORD PASS destructor_drains_and_empty_is_incomplete\n";
        {
            NativeFrameCapture c(root,root/"bounded");
            std::size_t accepted=0;
            while(c.submit(frame,accepted+1,state)) ++accepted;
            require(accepted<=NativeFrameCapture::default_frame_limit && !c.accepting(),"unbounded queue/session");
            c.finish();
            require(accepted==NativeFrameCapture::default_frame_limit || !c.error().empty(),"overrun silently dropped a frame");
            require(!fs::exists(root/"bounded"/"00600.ppm"),"frame cap exceeded");
        }
        {
            NativeFrameCapture c(root,root/"host-fail");
            c.submit(frame,1,state);c.invalidate("host capture exception");c.finish();
            require(read(root/"host-fail/recording.json").find("\"video_complete\":false")!=std::string::npos,"host failure lost");
            require(read(root/"host-fail/error.txt")=="host capture exception\n","failure reason lost");
        }
        std::cout<<"RECORD PASS bounded_queue_session_and_host_failure\n";
        rejects([&]{NativeFrameCapture c(root,root/"invalid-limit",0,6001);});
        rejects([&]{NativeFrameCapture c(root,root/"invalid-limit",0,0);});
        {
            NativeFrameCapture c(root,root/"two-frame-limit",123,2);
            require(c.submit(frame,1,state) && c.submit(frame,2,state) && !c.submit(frame,3,state),"configured cap");
            c.audioResult(true,true,true);c.finish();
            const auto metadata=read(root/"two-frame-limit/recording.json");
            require(metadata.find("\"frame_limit\":2")!=std::string::npos && metadata.find("\"qpc_origin_100ns\":123")!=std::string::npos &&
                    metadata.find("\"audio_captured\":true")!=std::string::npos,"A/V metadata");
        }
        std::cout<<"RECORD PASS configured_limit_and_av_metadata\n";
        {
            NativeFrameCapture c(root,root/"help-events");
            NativeFrameCapture::HelpModal before{},after{};
            std::array<std::uint8_t,32> hash{};hash[0]=0xab;
            c.helpEvent(0,0,0,0,false,before,before,state,hash);
            require(c.submit(frame,1,state),"Help event first frame");
            after[0]=5;c.helpEvent(2,3,128,0,false,before,after,state,hash);
            before=after;after[0]=0;c.helpEvent(2,3,0,3,false,before,after,state,hash);
            require(c.submit(frame,3,state),"Help event return frame");c.finish();
            const auto events=read(root/"help-events/help_events.csv");
            require(events.find("1,2,1,3,128,0,0,")!=std::string::npos && events.find("2,2,1,3,0,3,0,")!=std::string::npos,"between-paint events lost/reordered");
            require(events.find("ab00000000000000000000000000000000000000000000000000000000000000")!=std::string::npos,"slot hash encoding");
            require(read(root/"help-events/recording.json").find("\"help_events\":3")!=std::string::npos,"Help event summary");
        }
        for(unsigned reason=0;reason<3;++reason) {
            NativeFrameCapture c(root,root/("help-fail-"+std::to_string(reason)));
            NativeFrameCapture::HelpModal modal{};std::array<std::uint8_t,32> hash{};
            require(c.submit(frame,10,state),"Help failure setup");
            if(reason==0)c.helpEvent(9,0,0,0,false,modal,modal,state,hash);
            else if(reason==1)c.helpEvent(11,4,0,0,false,modal,modal,state,hash);
            else for(std::size_t i=0;i<=NativeFrameCapture::max_help_events;++i)c.helpEvent(11+i,0,0,0,false,modal,modal,state,hash);
            c.finish();require(!c.error().empty(),"Help bounds failure silently accepted");
        }
        std::cout<<"RECORD PASS bounded_help_events_same_frame_clock_and_hash\n";
        // Only this test's exclusive, resolved temporary child can be removed.
        require(fs::canonical(root).parent_path()==fs::canonical(fs::temp_directory_path()),"fixture escaped temp");
        fs::remove_all(root);
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"RECORD FAIL "<<e.what()<<" fixture="<<root<<'\n';return 1;
    }
}
