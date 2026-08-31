#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <windows.h>
#include <mmsystem.h>

namespace nba97 {
// Injectable WinMM boundary. Successful calls use MMSYSERR_NOERROR, never a
// Boolean cast of MMRESULT. Implementations and output owners are serialized.
class RecoveredWaveApi {
public:
    virtual ~RecoveredWaveApi() = default;
    virtual MMRESULT open(HWAVEOUT*,const WAVEFORMATEX*) noexcept = 0;
    virtual MMRESULT prepare(HWAVEOUT,WAVEHDR*) noexcept = 0;
    virtual MMRESULT write(HWAVEOUT,WAVEHDR*) noexcept = 0;
    virtual MMRESULT reset(HWAVEOUT) noexcept = 0;
    virtual MMRESULT unprepare(HWAVEOUT,WAVEHDR*) noexcept = 0;
    virtual MMRESULT close(HWAVEOUT) noexcept = 0;
};
enum class RecoveredWaveOperation { None,Open,Prepare,Write,Reset,AwaitReturn,Unprepare,Close };
struct RecoveredWaveFailure {
    RecoveredWaveOperation operation=RecoveredWaveOperation::None;
    // AwaitReturn + WAVERR_STILLPLAYING is our native pending result after a
    // successful reset without WHDR_DONE. Other codes are actual MMRESULTs.
    MMRESULT code=MMSYSERR_NOERROR;
};
struct RecoveredWaveProgress {
    std::uint64_t generation=0;
    bool submitted=false,returned=false,natural=false,interrupted=false,storage_released=false;
};

// Owns a stable heap session and a stable heap header/PCM generation. All calls,
// including collection, must be serialized by the host. No callback writes
// source/game state; generation numbers are native lifetime IDs, not original
// voice handles and never consume RNG. No actual device opens before play().
class RecoveredWaveOutput final {
public:
    RecoveredWaveOutput();
    explicit RecoveredWaveOutput(std::shared_ptr<RecoveredWaveApi>);
    ~RecoveredWaveOutput();
    RecoveredWaveOutput(const RecoveredWaveOutput&)=delete;
    RecoveredWaveOutput& operator=(const RecoveredWaveOutput&)=delete;
    std::uint64_t play(std::vector<std::int16_t> pcm,std::uint32_t sample_rate);
    void stop() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] RecoveredWaveProgress progress() const noexcept;
    [[nodiscard]] bool isCurrentGeneration(std::uint64_t) const noexcept;
    [[nodiscard]] RecoveredWaveFailure failure() const noexcept {return failure_;}
    // Destructor failure transfers ownership to an intrusive retained list,
    // with no allocation and no static owner teardown. Retry only on the same
    // serialized host thread. play/stop also collect opportunistically. Failed
    // entries remain alive; successful ones are deleted. The injected API
    // remains owned by each retained session.
    static std::size_t collectRetained() noexcept;
    static std::size_t retainedCount() noexcept;
private:
    struct Session;
    static Session*& retained() noexcept;
    static bool retire(Session&,bool keep_device,RecoveredWaveFailure&,
                       RecoveredWaveProgress&) noexcept;
    static void diagnose(RecoveredWaveFailure) noexcept;
    std::shared_ptr<RecoveredWaveApi> api_;
    std::unique_ptr<Session> session_;
    std::uint64_t next_generation_=0;
    RecoveredWaveFailure failure_{};
    RecoveredWaveProgress last_{};
};
} // namespace nba97
