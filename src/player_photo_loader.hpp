#pragma once
#include "psh_image.hpp"
#include "player_portrait_archive.hpp"
#include "recovered/player_photo.h"
#include <chrono>
#include <functional>
#include <future>
#include <stdexcept>
#include <utility>

namespace nba97 {
/* Native I/O adaptation, not recovered PS1 scheduling. At most one decode is
   running and one latest request is retained. Workers never touch UI state. */
class PlayerPhotoLoader {
public:
    using Decode = std::function<PshImage(const std::filesystem::path&)>;
    enum class Event { None, Ready, Failed, Stale };
    enum class RawChecksum { NotChecked, Accepted, Rejected };
    struct Result {
        Event event = Event::None;
        PshImage image;
        std::string error;
        std::int32_t record = -1;
        RawChecksum raw_checksum = RawChecksum::NotChecked;
        std::shared_ptr<const PlayerPortraitArchive> archive;
    };
    using BeforePublish = std::function<void(const Result&)>;
    explicit PlayerPhotoLoader(Decode decode);
    void reset();
    // Actual View Player route: source count chooses physical record and PNG.
    bool request(std::shared_ptr<const PlayerPortraitArchive>, std::int32_t logical_player,
                 std::filesystem::path png_directory);
    // Image-only adaptation retained for callers/tests with no raw archive.
    // Such results are NotChecked and NEVER invoke BeforePublish.
    bool request(std::int32_t record, std::filesystem::path path);
    const Nba97PlayerPhoto& state() const noexcept { return state_; }
    // wait=true is exclusively for deterministic offline captures/tests.
    // On the calling/UI thread, an accepted CURRENT raw request invokes the
    // callback before visibility publication, even when PNG decode failed.
    // Callback must not reenter this loader. Stale work cannot invoke it.
    Result poll(bool wait = false, const BeforePublish& before_publish = {});
private:
    bool queueRequest(std::int32_t record, std::filesystem::path,
                      std::shared_ptr<const PlayerPortraitArchive>);
    void launch();
    Decode decode_;
    Nba97PlayerPhoto state_{};
    std::uint64_t generation_ = 0, running_generation_ = 0;
    std::filesystem::path path_;
    std::shared_ptr<const PlayerPortraitArchive> archive_;
    std::future<Result> job_;
};
} // namespace nba97
