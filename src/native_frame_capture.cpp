#include "native_frame_capture.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace nba97 {
namespace {
bool below(const std::filesystem::path& root, const std::filesystem::path& path) {
    auto r=root.begin(), p=path.begin();
    for(;r!=root.end();++r,++p) if(p==path.end() || *r!=*p) return false;
    return p!=path.end();
}
}
NativeFrameCapture::NativeFrameCapture(const std::filesystem::path& private_root,
                                     const std::filesystem::path& destination,
                                     std::uint64_t qpc_origin100ns,std::size_t frame_limit):qpc_origin_(qpc_origin100ns),frame_limit_(frame_limit) {
    if(!frame_limit_ || frame_limit_>max_frames) throw std::runtime_error("native frame limit must be 1..6000");
    const auto root=std::filesystem::canonical(private_root);
    directory_=std::filesystem::weakly_canonical(std::filesystem::absolute(destination));
    if(!below(root,directory_)) throw std::runtime_error("native recording must stay below the private root");
    if(std::filesystem::exists(directory_)) throw std::runtime_error("native recording destination already exists; choose a fresh directory");
    std::filesystem::create_directories(directory_.parent_path());
    if(!std::filesystem::create_directory(directory_)) throw std::runtime_error("native recording directory was not created exclusively");
    frames_.open(directory_/"frames.csv",std::ios::binary);
    inputs_.open(directory_/"inputs.csv",std::ios::binary);
    help_events_.open(directory_/"help_events.csv",std::ios::binary);
    if(!frames_ || !inputs_ || !help_events_) throw std::runtime_error("cannot open native recording timelines");
    frames_<<"index,ns,boot,page,menu_ms,team,phase,child,help,cursor0,cursor1,top0,top1,player0,player1,fact_variant,fact_flash,transition\n";
    inputs_<<"ns,next_frame,message,code,data\n";
    help_events_<<"index,ns,next_frame,operation,raw,result,notice";
    for(const char* prefix:{"before_","after_"})
        for(const char* field:{"phase","x","y","width","height","target_x","target_y","target_width","target_height","held"})
            help_events_<<','<<prefix<<field;
    help_events_<<",boot,page,menu_ms,team,phase,child,help,cursor0,cursor1,top0,top1,player0,player1,fact_variant,fact_flash,transition,slots_sha256\n";
    thread_=std::thread(&NativeFrameCapture::worker,this);
}
NativeFrameCapture::~NativeFrameCapture() { try { finish(); } catch(...) {} }
bool NativeFrameCapture::accepting() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !closing_ && error_.empty() && submitted_<frame_limit_;
}
std::string NativeFrameCapture::error() const { std::lock_guard<std::mutex> lock(mutex_); return error_; }
std::size_t NativeFrameCapture::submitted() const { std::lock_guard<std::mutex> lock(mutex_); return submitted_; }
void NativeFrameCapture::fail(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(error_.empty()) error_=message;
    closing_=true;
    ready_.notify_one();
}
bool NativeFrameCapture::submit(const std::vector<std::uint8_t>& bgra,std::uint64_t ns,const State& state) {
    if(bgra.size()!=frame_bytes) { fail("invalid framebuffer extent"); return false; }
    std::lock_guard<std::mutex> lock(mutex_);
    if(closing_ || !error_.empty() || submitted_>=frame_limit_) return false;
    if((submitted_ && ns<=last_frame_ns_) || queue_.size()>=queue_capacity) {
        error_=queue_.size()>=queue_capacity ? "writer queue overrun; capture is incomplete" : "nonmonotonic presentation timestamp";
        closing_=true;ready_.notify_one();return false;
    }
    queue_.push_back(Packet{submitted_,ns,state,bgra});
    ++submitted_;last_frame_ns_=ns;
    ready_.notify_one();return true;
}
void NativeFrameCapture::input(std::uint64_t ns,std::uint32_t message,std::uint64_t code,std::int64_t data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(closing_ || !error_.empty() || submitted_>=frame_limit_) return;
    if((input_count_ && ns<last_input_ns_) || input_count_>=100000) {
        error_="invalid or excessive input timeline";closing_=true;ready_.notify_one();return;
    }
    inputs_<<ns<<','<<submitted_<<','<<message<<','<<code<<','<<data<<'\n';
    if(!inputs_) { error_="input timeline write failed";closing_=true;ready_.notify_one();return; }
    ++input_count_;last_input_ns_=ns;
}
void NativeFrameCapture::worker() {
    try {
        std::vector<char> rgb(512*240*3);
        for(;;) {
            Packet packet;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock,[&]{return closing_ || !queue_.empty();});
                if(queue_.empty()) break;
                packet=std::move(queue_.front());queue_.pop_front();
            }
            for(std::size_t src=0,dst=0;src<packet.bgra.size();src+=4,dst+=3) {
                rgb[dst]=static_cast<char>(packet.bgra[src+2]);
                rgb[dst+1]=static_cast<char>(packet.bgra[src+1]);
                rgb[dst+2]=static_cast<char>(packet.bgra[src]);
            }
            std::ostringstream filename;filename<<std::setfill('0')<<std::setw(5)<<packet.index<<".ppm";
            std::ofstream frame(directory_/filename.str(),std::ios::binary);
            frame<<"P6\n512 240\n255\n";
            frame.write(rgb.data(),static_cast<std::streamsize>(rgb.size()));frame.close();
            if(!frame) throw std::runtime_error("frame write failed");
            frames_<<packet.index<<','<<packet.ns;
            for(auto value:packet.state) frames_<<','<<value;
            frames_<<'\n';frames_.flush();
            if(!frames_) throw std::runtime_error("frame timeline write failed");
            ++written_;
        }
    } catch(const std::exception& e) { fail(e.what()); }
    catch(...) { fail("unknown writer failure"); }
}
void NativeFrameCapture::helpEvent(std::uint64_t ns,unsigned operation,unsigned raw,unsigned result,bool notice,
                                  const HelpModal& before,const HelpModal& after,const State& state,
                                  const std::array<std::uint8_t,32>& slots_hash) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(closing_ || !error_.empty() || submitted_>=frame_limit_)return;
    if(operation>3 || raw>65535 || result>3 || (help_count_ && ns<last_help_ns_) ||
       (submitted_ && ns<last_frame_ns_) || help_count_>=max_help_events) {
        error_="invalid or excessive Help event timeline";closing_=true;ready_.notify_one();return;
    }
    help_events_<<help_count_<<','<<ns<<','<<submitted_<<','<<operation<<','<<raw<<','<<result<<','<<notice;
    for(auto value:before)help_events_<<','<<value;
    for(auto value:after)help_events_<<','<<value;
    for(auto value:state)help_events_<<','<<value;
    help_events_<<',';
    constexpr char hex[]="0123456789abcdef";
    for(auto value:slots_hash)help_events_<<hex[value>>4]<<hex[value&15];
    help_events_<<'\n';
    if(!help_events_){error_="Help event write failed";closing_=true;ready_.notify_one();return;}
    ++help_count_;last_help_ns_=ns;
}
void NativeFrameCapture::finish() {
    if(finished_) return;
    {std::lock_guard<std::mutex> lock(mutex_);closing_=true;}
    ready_.notify_one();
    if(thread_.joinable()) thread_.join();
    inputs_.close();frames_.close();help_events_.close();
    if(!inputs_ || !frames_ || !help_events_) fail("timeline close failed");
    if(!submitted_) fail("no native frames presented");
    if(written_!=submitted_) fail("not all submitted frames were written");
    // Deliberately not the original-reference manifest schema. Variable-rate
    // presentations, native events and optional mixed audio do not establish parity.
    std::ofstream report(directory_/"recording.json",std::ios::binary);
    report<<"{\n  \"schema_version\":1,\n  \"kind\":\"native_presentations\",\n"
          <<"  \"video_complete\":"<<(error().empty()?"true":"false")<<",\n"
          <<"  \"audio_requested\":"<<(audio_requested_?"true":"false")<<",\n"
          <<"  \"audio_captured\":"<<(audio_captured_?"true":"false")<<",\n"
          <<"  \"audio_complete\":"<<(audio_complete_?"true":"false")<<",\n"
          <<"  \"clock\":\""<<(qpc_origin_?"qpc_100ns_since_start_scaled_to_ns":"steady_clock_nanoseconds_since_start")<<"\",\n"
          <<"  \"qpc_origin_100ns\":"<<qpc_origin_<<",\n"
          <<"  \"frame_rate\":null,\n  \"scanout_verified\":false,\n  \"reference_ready\":false,\n"
          <<"  \"submitted\":"<<submitted_<<",\n  \"written\":"<<written_
          <<",\n  \"inputs\":"<<input_count_<<",\n  \"frame_limit\":"<<frame_limit_
          <<",\n  \"help_event_schema\":1,\n  \"help_events\":"<<help_count_
          <<",\n  \"queue_capacity\":"<<queue_capacity<<"\n}\n";
    report.close();
    if(!report) { fail("recording summary write failed");throw std::runtime_error(error()); }
    if(!error().empty()) {
        std::ofstream reason(directory_/"error.txt",std::ios::binary);
        reason<<error()<<'\n';reason.close();
    }
    finished_=true;
}
}
