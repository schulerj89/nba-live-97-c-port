#pragma once
#include <array>
#include <cstddef>

namespace nba97 {
// Driver calls report success only on MMSYSERR_NOERROR. A failed operation
// NEVER releases storage: reset-returned buffers are not natural completion.
class MusicRetirementOps {
public:
    virtual ~MusicRetirementOps() = default;
    virtual bool reset() noexcept = 0;
    virtual bool unprepare(std::size_t index) noexcept = 0;
    virtual bool close() noexcept = 0;
};
struct MusicBufferRetirement {
    bool opened = false;
    std::array<bool, 4> prepared{};
    bool release(MusicRetirementOps& ops) noexcept {
        if (!opened) return true;
        if (!ops.reset()) return false;
        bool complete = true;
        for (std::size_t i = 0; i < prepared.size(); ++i) {
            if (prepared[i]) {
                if (ops.unprepare(i)) prepared[i] = false;
                else complete = false;
            }
        }
        if (!complete || !ops.close()) return false;
        opened = false;
        return true;
    }
};
} // namespace nba97
