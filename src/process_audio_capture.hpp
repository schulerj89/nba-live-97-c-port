#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace nba97 {
// Windows process-loopback only: current or explicitly verified process tree,
// never microphone or whole-system fallback. PCM is Windows mix, not raw SPU.
class ProcessAudioCapture final {
public:
    static std::uint64_t qpc100ns();
    ProcessAudioCapture(const std::filesystem::path& fresh_directory, std::uint64_t origin100ns);
    // Diagnostic-only explicit target: PID must still identify expected_executable.
    // A retained process handle/creation timestamp prevents silent retargeting;
    // target exit fails the recording. The normal app uses the constructor above.
    ProcessAudioCapture(const std::filesystem::path& fresh_directory, std::uint64_t origin100ns,
                        std::uint32_t target_pid, const std::filesystem::path& expected_executable);
    ~ProcessAudioCapture();
    ProcessAudioCapture(const ProcessAudioCapture&)=delete;
    ProcessAudioCapture& operator=(const ProcessAudioCapture&)=delete;
    void finish();
    bool complete() const;
    std::string error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
