#include "player_photo_loader.hpp"
#include <atomic>
#include <iostream>

namespace {
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
PshImage photo(unsigned char marker = 1) {
    PshImage p; p.width=180; p.height=156; p.rgba.assign(180*156*4,marker); return p;
}
}
int main() {
    try {
        Nba97PlayerPhoto s{}; nba97_player_photo_reset(&s);
        check(s.record==-1 && !s.photo_enabled && !s.city_enabled, "initial layout");
        check(!nba97_player_photo_request(&s,-1), "invalid request");
        check(nba97_player_photo_request(&s,38) && s.pending && !s.city_enabled, "initial request");
        check(!nba97_player_photo_request(&s,38) && s.pending, "pending duplicate");
        nba97_player_photo_complete(&s,1);
        check(s.photo_enabled && s.city_enabled && !s.pending, "success enables both");
        check(!nba97_player_photo_request(&s,38) && s.photo_enabled, "ready duplicate");
        check(nba97_player_photo_request(&s,40) && !s.photo_enabled && s.city_enabled, "cycle keeps city");
        nba97_player_photo_complete(&s,0);
        check(!s.photo_enabled && s.city_enabled && !s.pending, "failure keeps wait and city");
        nba97_player_photo_reset(&s); nba97_player_photo_complete(&s,1);
        check(!s.photo_enabled && !s.city_enabled, "completion after reset ignored");
        std::cout << "PHOTO PASS original request/duplicate/success/failure/city visibility branches\n";

        using Loader=nba97::PlayerPhotoLoader;
        std::promise<void> gate, started;
        auto released=gate.get_future().share();
        std::atomic<int> calls{0};
        Loader loader([&](const std::filesystem::path& path) {
            const auto n=++calls;
            if(n==1) {started.set_value(); released.wait();}
            return photo(static_cast<unsigned char>(std::stoi(path.string())));
        });
        loader.request(38,"38"); started.get_future().wait();
        check(loader.poll().event==Loader::Event::None, "UI poll must not block");
        loader.request(39,"39"); loader.request(40,"40");
        check(calls==1 && loader.state().pending, "bounded queue while worker is busy");
        gate.set_value();
        check(loader.poll(true).event==Loader::Event::Stale, "discard replaced result");
        auto ready=loader.poll(true);
        check(ready.event==Loader::Event::Ready && ready.record==40 && ready.image.rgba[0]==40,
              "publish latest requested identity only");
        check(calls==2 && loader.state().city_enabled, "intermediate request coalesced");
        check(!loader.request(40,"40") && calls==2, "duplicate does not decode");
        loader.request(41,"41"); loader.reset();
        check(loader.poll(true).event==Loader::Event::Stale && !loader.state().photo_enabled &&
              !loader.state().city_enabled, "exit cancels publication");
        loader.request(42,"42"); loader.reset(); loader.request(43,"43");
        check(loader.poll(true).event==Loader::Event::Stale, "reentry stale request");
        ready=loader.poll(true);
        check(ready.event==Loader::Event::Ready && ready.record==43, "reentry latest photo");
        std::cout << "PHOTO PASS nonblocking/bounded/latest-only/duplicate/close/reentry host guards\n";

        Loader bad([](const std::filesystem::path& path) {
            if(path=="throw") throw std::runtime_error("fixture missing photo");
            auto p=photo(); if(path=="size") p.width=179; else p.rgba.pop_back(); return p;
        });
        for(const auto* path : {"throw","size","pixels"}) {
            bad.reset(); bad.request(0,path);
            const auto failure=bad.poll(true);
            check(failure.event==Loader::Event::Failed && !failure.error.empty() &&
                  failure.image.rgba.empty() && !bad.state().photo_enabled && !bad.state().city_enabled,
                  "bad asset must leave wait, not publish invalid pixels");
        }
        std::cout << "PHOTO PASS decoder error/dimensions/pixel-size validation; no assets required\n";
        return 0;
    } catch(const std::exception& e) {std::cerr << "PHOTO FAIL " << e.what() << '\n'; return 1;}
}
