#include "player_photo_loader.hpp"
#include <iomanip>
#include <sstream>

namespace nba97 {
PlayerPhotoLoader::PlayerPhotoLoader(Decode decode) : decode_(std::move(decode)) {
    nba97_player_photo_reset(&state_);
}
void PlayerPhotoLoader::reset() {
    ++generation_;
    nba97_player_photo_reset(&state_);
    path_.clear();
    archive_.reset();
}
bool PlayerPhotoLoader::request(std::shared_ptr<const PlayerPortraitArchive> archive,
    std::int32_t logical_player, std::filesystem::path png_directory) {
    if (!archive) throw std::runtime_error("View Player request has no owned Z1PORT archive");
    const auto physical = archive->physicalRecord(logical_player);
    std::ostringstream name;
    name << "player_" << std::setfill('0') << std::setw(3) << physical << ".png";
    return queueRequest(static_cast<std::int32_t>(physical), png_directory / name.str(), std::move(archive));
}
bool PlayerPhotoLoader::request(std::int32_t record, std::filesystem::path path) {
    return queueRequest(record, std::move(path), {});
}
bool PlayerPhotoLoader::queueRequest(std::int32_t record, std::filesystem::path path,
    std::shared_ptr<const PlayerPortraitArchive> archive) {
    if (!nba97_player_photo_request(&state_, record)) return false;
    ++generation_;
    path_ = std::move(path);
    archive_ = std::move(archive);
    if (!job_.valid()) launch();
    return true;
}
PlayerPhotoLoader::Result PlayerPhotoLoader::poll(bool wait, const BeforePublish& before_publish) {
    if (!job_.valid()) return {};
    if (!wait && job_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return {};
    auto result = job_.get();
    if (running_generation_ != generation_) {
        result.image = {};
        result.event = Event::Stale;
        result.raw_checksum = RawChecksum::NotChecked;
        result.archive.reset();
        if (state_.pending) launch();
        return result;
    }
    //30EFC clears music selection before30F70 texture decode/publication. Native
    // PNG work occurs on the worker, but no visibility state has been published
    // yet. Retain this effect even if the worker's later PNG stage failed.
    try {
        if (result.raw_checksum == RawChecksum::Accepted && before_publish) before_publish(result);
    } catch (...) {
        nba97_player_photo_complete(&state_, 0);
        throw;
    }
    nba97_player_photo_complete(&state_, result.event == Event::Ready);
    return result;
}
void PlayerPhotoLoader::launch() {
    running_generation_ = generation_;
    const auto record = state_.record;
    try {
        // Captured immutable ownership outlives reset/replacement. Workers
        // calculate results only; they never touch live music or UI state.
        job_ = std::async(std::launch::async,
            [decode=decode_, path=path_, archive=archive_, record] {
                Result r; r.record = record; r.archive = archive;
                try {
                    if (archive) {
                        if (!archive->checksumAccepted(static_cast<std::uint32_t>(record))) {
                            r.raw_checksum = RawChecksum::Rejected;
                            throw std::runtime_error("original Z1PORT raw slice checksum rejected; no PNG publication");
                        }
                        r.raw_checksum = RawChecksum::Accepted;
                    }
                    r.image = decode(path);
                    if (r.image.width != 180 || r.image.height != 156 ||
                        r.image.rgba.size() != 180u * 156u * 4u)
                        throw std::runtime_error("invalid Z1PORT portrait: expected 180x156 RGBA");
                    r.event = Event::Ready;
                } catch (const std::exception& e) {
                    r.image = {}; r.error = e.what(); r.event = Event::Failed;
                } catch (...) {
                    r.image = {}; r.error = "unknown portrait decoder failure"; r.event = Event::Failed;
                }
                return r;
            });
    } catch (...) {
        nba97_player_photo_complete(&state_, 0);
        throw;
    }
}
} // namespace nba97
