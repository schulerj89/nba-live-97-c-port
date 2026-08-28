#pragma once
#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nba97 {
// Passive evidence collector, not a game clock/replay engine or audio recorder.
// Exactly the submitted, unscaled 512x240 BGRA presentations are retained as RGB.
// No frame synthesis, deduplication, fixed-fps claim, or reference-manifest credit.
class NativeFrameCapture final {
public:
    static constexpr std::size_t frame_bytes = 512 * 240 * 4;
    static constexpr std::size_t max_frames = 6000;
    static constexpr std::size_t default_frame_limit = 600;
    static constexpr std::size_t queue_capacity = 8;
    // boot,page,menu_ms,team,phase,child,help,cursor0,cursor1,top0,top1,
    // player0,player1,fact_variant,fact_flash,transition
    using State = std::array<std::int64_t,16>;
    // phase,rect x/y/w/h,target x/y/w/h,held. These are observed controller
    // boundaries, not synthesized frames or original execution claims.
    using HelpModal = std::array<std::int64_t,10>;
    static constexpr std::size_t max_help_events=10000;
    NativeFrameCapture(const std::filesystem::path& private_root,
                       const std::filesystem::path& destination,
                       std::uint64_t qpc_origin100ns=0,
                       std::size_t frame_limit=default_frame_limit);
    ~NativeFrameCapture();
    NativeFrameCapture(const NativeFrameCapture&) = delete;
    NativeFrameCapture& operator=(const NativeFrameCapture&) = delete;
    bool submit(const std::vector<std::uint8_t>& bgra, std::uint64_t ns, const State& state);
    void input(std::uint64_t ns, std::uint32_t message, std::uint64_t code, std::int64_t data);
    void helpEvent(std::uint64_t ns,unsigned operation,unsigned raw,unsigned result,bool notice,
                   const HelpModal& before,const HelpModal& after,const State& state,
                   const std::array<std::uint8_t,32>& slots_hash);
    void finish();
    void invalidate(const std::string& reason) { fail(reason); }
    void audioResult(bool requested,bool captured,bool complete) noexcept {
        audio_requested_=requested;audio_captured_=captured;audio_complete_=complete;
    }
    bool accepting() const;
    std::string error() const;
    std::size_t submitted() const;
    const std::filesystem::path& directory() const noexcept { return directory_; }
private:
    struct Packet { std::size_t index; std::uint64_t ns; State state; std::vector<std::uint8_t> bgra; };
    void worker();
    void fail(const std::string& message);
    std::filesystem::path directory_;
    std::ofstream frames_, inputs_, help_events_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Packet> queue_;
    std::thread thread_;
    std::string error_;
    std::size_t submitted_=0, written_=0, input_count_=0;
    std::size_t help_count_=0;
    std::uint64_t last_help_ns_=0;
    std::uint64_t last_frame_ns_=0, last_input_ns_=0;
    bool closing_=false, finished_=false;
    std::uint64_t qpc_origin_=0;
    bool audio_requested_=false,audio_captured_=false,audio_complete_=false;
    std::size_t frame_limit_=default_frame_limit;
};
}
