#include "player_photo_loader.hpp"
#include "recovered/frontend_resource.h"
#include "recovered/music_routing.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {
void check(bool ok) { if (!ok) std::abort(); }
void word(std::vector<std::uint8_t>& b,std::uint32_t value) {
    for(unsigned shift=0;shift<32;shift+=8) b.push_back(static_cast<std::uint8_t>(value>>shift));
}
std::shared_ptr<const nba97::PlayerPortraitArchive> archive(bool corrupt_last=false) {
    std::vector<std::uint8_t> index,raw;word(index,2);
    for(unsigned record=0;record<3;++record) {
        const std::uint8_t data[]={static_cast<std::uint8_t>(record),23,67,99};
        std::uint16_t checksum=0;check(nba97_resource_crc16(data,4,&checksum)!=0);
        word(index,6);word(index,static_cast<std::uint32_t>(raw.size()));
        raw.insert(raw.end(),std::begin(data),std::end(data));
        raw.push_back(static_cast<std::uint8_t>(checksum));raw.push_back(static_cast<std::uint8_t>(checksum>>8));
    }
    if(corrupt_last) raw.back()^=1;
    return nba97::PlayerPortraitArchive::fromBytes(std::move(index),std::move(raw));
}
PshImage image(const std::filesystem::path& path) {
    PshImage p;p.width=180;p.height=156;p.source=path;p.rgba.assign(180u*156u*4u,37);return p;
}
} // namespace
int main() {
    using Loader=nba97::PlayerPhotoLoader;
    const auto ui_thread=std::this_thread::get_id();
    auto bank=archive();
    Nba97MusicInputs music{};music.selection_blocked=77;
    std::atomic<unsigned> decodes{0};unsigned callbacks=0;
    Loader good([&](const std::filesystem::path& path) {
        check(std::this_thread::get_id()!=ui_thread);++decodes;return image(path);
    });
    auto before=[&](const Loader::Result& r) {
        check(std::this_thread::get_id()==ui_thread && good.state().pending && !good.state().photo_enabled);
        check(r.raw_checksum==Loader::RawChecksum::Accepted && r.archive);
        check(r.archive->acceptChecksum(static_cast<std::uint32_t>(r.record),&music.selection_blocked)==1);
        ++callbacks;
    };
    check(good.request(bank,1,"photos")); // Highest logical ID maps to final physical2.
    auto r=good.poll(true,before);
    check(r.event==Loader::Event::Ready && r.record==2 && r.image.source.filename()=="player_002.png");
    check(!music.selection_blocked && callbacks==1 && good.state().photo_enabled && good.state().city_enabled);
    check(!good.request(bank,1,"photos") && decodes==1);
    check(good.request(bank,2,"photos"));r=good.poll(true,before);
    check(r.record==0 && r.image.source.filename()=="player_000.png" && callbacks==2);

    // A valid raw checksum clears the shared music input even if the later
    // native PNG decoder fails. The clear happens before visibility changes.
    for(unsigned failure=0;failure<3;++failure) {
        Loader bad_png([&](const std::filesystem::path& path) {
            if(failure==0) throw std::runtime_error("fixture missing PNG");
            auto p=image(path);if(failure==1)p.width=179;else p.rgba.pop_back();return p;
        });
        music.selection_blocked=77;unsigned accepted=0;
        check(bad_png.request(bank,0,"photos"));
        r=bad_png.poll(true,[&](const Loader::Result& accepted_result) {
            check(bad_png.state().pending && !bad_png.state().photo_enabled);
            check(accepted_result.event==Loader::Event::Failed);
            check(accepted_result.archive->acceptChecksum(static_cast<std::uint32_t>(accepted_result.record),&music.selection_blocked)==1);
            ++accepted;
        });
        check(r.event==Loader::Event::Failed && r.raw_checksum==Loader::RawChecksum::Accepted);
        check(!music.selection_blocked && accepted==1 && !bad_png.state().pending && !bad_png.state().photo_enabled);
    }
    Loader corrupt([&](const std::filesystem::path& path) {++decodes;return image(path);});
    const auto before_corrupt=decodes.load();music.selection_blocked=77;
    check(corrupt.request(archive(true),1,"photos"));
    r=corrupt.poll(true,[&](const Loader::Result&) {std::abort();});
    check(r.event==Loader::Event::Failed && r.raw_checksum==Loader::RawChecksum::Rejected);
    check(decodes==before_corrupt && music.selection_blocked==77 && !corrupt.state().photo_enabled);

    // Accepted old raw data is insufficient after replacement/exit. At most one
    // worker and the latest pending request survive; callbacks cannot be stale.
    std::promise<void> started,release;
    auto released=release.get_future().share();std::atomic<unsigned> jobs{0};
    Loader latest([&](const std::filesystem::path& path) {
        if(++jobs==1) {started.set_value();released.wait();}return image(path);
    });
    check(latest.request(bank,0,"photos"));started.get_future().wait();
    check(latest.poll().event==Loader::Event::None);
    check(latest.request(bank,1,"photos"));check(latest.request(bank,2,"photos"));
    music.selection_blocked=77;release.set_value();
    r=latest.poll(true,[&](const Loader::Result&) {std::abort();});
    check(r.event==Loader::Event::Stale && r.raw_checksum==Loader::RawChecksum::NotChecked && !r.archive);
    check(music.selection_blocked==77 && jobs<=2 && latest.state().pending);
    r=latest.poll(true,[&](const Loader::Result& current) {
        check(current.record==0 && latest.state().pending);
        check(current.archive->acceptChecksum(0,&music.selection_blocked)==1);
    });
    check(r.event==Loader::Event::Ready && r.record==0 && jobs==2 && !music.selection_blocked);
    check(latest.request(bank,0,"photos"));latest.reset();music.selection_blocked=77;
    r=latest.poll(true,[&](const Loader::Result&) {std::abort();});
    check(r.event==Loader::Event::Stale && music.selection_blocked==77 && !latest.state().city_enabled);

    std::promise<void> lifetime_started,lifetime_release;
    auto lifetime_gate=lifetime_release.get_future().share();
    auto owned=archive();std::weak_ptr<const nba97::PlayerPortraitArchive> weak=owned;
    Loader lifetime([&](const std::filesystem::path& path) {
        lifetime_started.set_value();lifetime_gate.wait();return image(path);
    });
    check(lifetime.request(owned,0,"photos"));lifetime_started.get_future().wait();
    owned.reset();lifetime.reset();check(!weak.expired());lifetime_release.set_value();
    r=lifetime.poll(true,[&](const Loader::Result&) {std::abort();});
    check(r.event==Loader::Event::Stale && weak.expired());

    Loader unchecked([](const std::filesystem::path& path) {return image(path);});
    unchecked.request(17,"fixture.png");
    r=unchecked.poll(true,[&](const Loader::Result&) {std::abort();});
    check(r.event==Loader::Event::Ready && r.raw_checksum==Loader::RawChecksum::NotChecked);
    std::cout<<"Raw portrait acceptance, PNG failure ordering, cancellation, thread and immutable lifetime tests passed\n";
}
