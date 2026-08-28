#pragma once
#include "psh_image.hpp"
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
    struct Result {
        Event event = Event::None;
        PshImage image;
        std::string error;
        std::int32_t record = -1;
    };
    explicit PlayerPhotoLoader(Decode decode) : decode_(std::move(decode)) {
        nba97_player_photo_reset(&state_);
    }
    void reset() {
        ++generation_;
        nba97_player_photo_reset(&state_);
        path_.clear();
    }
    bool request(std::int32_t record, std::filesystem::path path) {
        if (!nba97_player_photo_request(&state_, record)) return false;
        ++generation_;
        path_ = std::move(path);
        if (!job_.valid()) launch();
        return true;
    }
    const Nba97PlayerPhoto& state() const noexcept { return state_; }
    // wait=true is exclusively for deterministic offline captures/tests.
    Result poll(bool wait = false) {
        if (!job_.valid()) return {};
        if (!wait && job_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return {};
        auto result = job_.get();
        if (running_generation_ != generation_) {
            result.image = {};
            result.event = Event::Stale;
            if (state_.pending) launch();
            return result;
        }
        nba97_player_photo_complete(&state_, result.event == Event::Ready);
        return result;
    }
private:
    void launch() {
        running_generation_ = generation_;
        // Capture values: destruction waits for only the bounded current job,
        // and changing/closing a card cannot invalidate worker-owned input.
        const auto record = state_.record;
        try {
            job_ = std::async(std::launch::async, [decode=decode_, path=path_, record] {
                Result r; r.record = record;
                try {
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
    Decode decode_;
    Nba97PlayerPhoto state_{};
    std::uint64_t generation_ = 0, running_generation_ = 0;
    std::filesystem::path path_;
    std::future<Result> job_;
};
} // namespace nba97
