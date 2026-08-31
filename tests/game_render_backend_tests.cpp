#include "game_render_backend.hpp"
#include "recovered/game_head_cache.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace nba97;
using R = GameRenderBackendResult;
unsigned checks = 0;
void check(bool ok, const char* why) {
    ++checks;
    if (!ok) throw std::runtime_error(std::to_string(checks) + ": " + why);
}
void put16(std::uint8_t* p, std::uint16_t value) { p[0] = static_cast<std::uint8_t>(value); p[1] = static_cast<std::uint8_t>(value >> 8); }
void put32(std::uint8_t* p, std::uint32_t value) { put16(p, static_cast<std::uint16_t>(value)); put16(p + 2, static_cast<std::uint16_t>(value >> 16)); }
std::uint16_t get16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8)); }
Nba97GameImageMemory describe(GameRenderMemory& owner, Nba97GameRenderBuffer view) {
    Nba97GameImageMemory memory{};
    check(owner.describe(view, memory), "describe owned allocation");
    return memory;
}
Nba97GameRenderBuffer data(GameRenderMemory& owner, std::vector<std::uint8_t> bytes) {
    const auto size = bytes.size();
    const auto id = owner.add(std::move(bytes), {}, 0);
    check(id != 0, "retain known allocation");
    return owner.knownBuffer(id, 0, size);
}
void memoryAndAliases() {
    GameRenderMemory source;
    const auto id = source.add(std::vector<std::uint8_t>(64, 0xa5), {}, 2);
    check(id != 0, "allocation");
    auto a = source.buffer(id, 4, 32), b = source.buffer(id, 12, 16);
    auto whole = source.buffer(id, 0, 64);
    GameRenderMemory clone(source);
    Nba97GameRenderBuffer ca{}, cb{};
    check(clone.rebind(source, a, ca) && clone.rebind(source, b, cb), "alias rebind");
    check(cb.data == ca.data + 8 && ca.data != a.data, "clone preserves aliases without sharing original");
    cb.data[3] = 0x39;
    check(ca.data[11] == 0x39 && a.data[11] == 0xa5, "mutation follows clone alias only");
    Nba97GameRenderImage image{};
    check(clone.rebind(source, Nba97GameRenderImage{whole, 40}, image), "image keeps enclosing allocation");
    check(image.offset == 40 && image.storage.data + 4 == ca.data, "backward image references retained");
    auto desc = describe(clone, cb);
    check(desc.address_mod4_known == 1 && desc.address_mod4 == 2, "original alignment includes subview offset");
    check(!clone.rebind(source, Nba97GameRenderBuffer{a.data, 100}, cb), "refuse beyond allocation");
    GameRenderMemory unrelated;
    unrelated.add(std::vector<std::uint8_t>(64), {}, 2);
    check(!unrelated.rebind(source, a, cb), "same ID in unrelated registry is not same resource");
    // A fork may independently append a same-size allocation in the same slot.
    const auto left = source.add(std::vector<std::uint8_t>(8), {}, 0);
    clone.add(std::vector<std::uint8_t>(8), {}, 0);
    check(!clone.rebind(source, source.buffer(left, 0, 8), cb), "fork additions cannot alias unrelated resources");
    const auto unknown = source.add(std::vector<std::uint8_t>(4, 0x77), {1,0,1,1});
    check(source.knownBuffer(unknown, 0, 4).data == nullptr, "old render owners cannot read unknown bytes");
    check(source.knownBuffer(unknown, 2, 2).data != nullptr, "known subview remains available");
    check(source.add({1,2}, {1,2}, 0) == 0 && source.add({1,2}, {1}, 0) == 0,
        "noncanonical or wrong-sized knownness rejected");
    GameRenderMemory assigned;
    assigned = source;
    check(assigned.rebind(source, a, ca), "copy assignment keeps resource identities");
    assigned = assigned;
    check(assigned.rebind(source, a, ca), "self assignment");
}

void rawTransfers() {
    GameRenderBackend zero;
    Nba97GameImageTransfer empty{};empty.rect={-200,30000,0,1};
    check(GameRenderBackend::transferIo(&zero,&empty)==0&&zero.lastResult==R::SdkLimitsUnknown,"zero SDK upload still requires limit provenance");
    zero.sdkTransferLimitsKnown=true;zero.sdkTransferWidth=1024;zero.sdkTransferHeight=512;
    for(int w:{-32768,-1,0,1,1024,32767})for(int h:{-32768,-1,0,1,512,32767})if(w<=0||h<=0){
        empty.rect.w=static_cast<std::int16_t>(w);empty.rect.h=static_cast<std::int16_t>(h);
        check(GameRenderBackend::transferIo(&zero,&empty)==1&&zero.lastResult==R::Complete,"SDK empty upload returns before source/mask/coordinates");
    }
    std::uint16_t untouched=9;check(!zero.vram.word(0,0,untouched)&&untouched==9,"empty SDK transfer never establishes pixels");
    GameVramWords vram;
    std::uint16_t value = 0xbeef;
    check(!vram.word(0, 0, value) && value == 0xbeef, "initial VRAM unknown, output untouched");
    std::vector<std::uint8_t> bytes{0x34,0x12,0xcd,0xab,0x02,0x80,0x99,0x99};
    Nba97GameImageMemory src{bytes.data(),nullptr,bytes.size(),0,1};
    check(vram.upload({1021,511,3,1},{&src,0}) == R::Complete, "edge odd upload with CPU padding");
    check(vram.word(1021,511,value) && value == 0x1234, "little endian raw word");
    check(vram.word(1023,511,value) && value == 0x8002, "bit15 preserved");
    check(!vram.word(0,0,value), "padding is not a wrapped VRAM pixel");
    src.size = 6;
    check(vram.upload({20,20,3,1},{&src,0}) == R::Resource, "odd upload requires DMA padding bytes");
    check(!vram.word(20,20,value), "refused upload atomic");
    src.size = 8;
    std::array<std::uint8_t,8> known{1,1,1,1,1,1,0,0};
    src.known = known.data();
    check(vram.upload({20,20,3,1},{&src,0}) == R::Unknown, "padding read must also be known");
    known[7] = 2;
    check(vram.upload({20,20,3,1},{&src,0}) == R::Argument, "unknown prefix cannot hide malformed later byte");
    src.known = nullptr;
    src.address_mod4_known = 0;
    check(vram.upload({20,20,2,1},{&src,0}) == R::AlignmentUnknown, "no heap-alignment inference");
    src.address_mod4_known = 1;
    check(vram.upload({20,20,2,1},{&src,2}) == R::AlignmentTrap, "original unaligned CPU word read");
    std::array<std::uint8_t,66> dmaBytes{};
    Nba97GameImageMemory dma{dmaBytes.data(),nullptr,dmaBytes.size(),0,1};
    check(vram.upload({20,20,32,1},{&dma,2}) == R::DmaAlignmentUnsupported,
        "pure DMA misalignment is unsupported, not a proved CPU load trap");
    check(vram.store({20,20,32,1},{&dma,2}) == R::DmaAlignmentUnsupported,
        "pure DMA readback misalignment is not a proved CPU store trap");
    check(vram.upload({1023,511,2,1},{&src,0}) == R::RectangleUnsupported, "no wrapping invention");
    check(vram.upload({0,0,0,1},{&src,0}) == R::RectangleUnsupported, "zero upload not guessed");
    check(vram.move({1021,511,2,1},0,0) == R::Complete, "disjoint transfer");
    check(vram.word(0,0,value) && value == 0x1234, "moved raw data");
    check(vram.move({0,0,2,1},1,0) == R::OverlapUnsupported, "partial overlap unsupported");
    check(vram.move({0,0,2,1},0,0) == R::OverlapUnsupported, "self-overlap not silently optimized");
    std::array<std::uint8_t,8> out{}, outKnown{};
    out.fill(0xcc);
    Nba97GameImageMemory dst{out.data(),outKnown.data(),out.size(),0,1};
    check(vram.store({0,0,2,1},{&dst,0}) == R::Complete, "actual raw readback");
    check(get16(out.data()) == 0x1234 && get16(out.data()+2) == 0xabcd && out[4] == 0xcc,
        "readback writes exact byte extent");
    check(outKnown[0] == 1 && outKnown[3] == 1 && outKnown[4] == 0, "readback establishes knownness");
    const auto saved = out;
    check(vram.store({0,0,3,1},{&dst,0}) == R::ReadbackPaddingUnsupported && out == saved,
        "unproved odd readback padding refuses");
    check(vram.store({0,0,2,2},{&dst,0}) == R::Unknown && out == saved,
        "unknown later row does not partially overwrite CPU destination");
    check(vram.move({10,10,2,1},0,0) == R::Complete && !vram.word(0,0,value),
        "moving unknown pixels does not manufacture known zero");

    // Independent rectangular readback oracle: source words encode absolute
    // coordinates. Scatter 128 differently sized rectangles, then check every
    // resulting known word against a separate flat expected plane.
    GameRenderMemory allocations;
    std::vector<std::uint16_t> expected(GameVramWords::Width * GameVramWords::Height);
    std::vector<bool> marked(expected.size());
    std::uint32_t rng = 0x1997;
    auto random = [&] { rng = rng * 1664525u + 1013904223u; return rng; };
    for (unsigned n = 0; n < 128; ++n) {
        const int w = 2 * (1 + static_cast<int>(random() % 32)), h = 1 + static_cast<int>(random() % 25);
        const int x = static_cast<int>(random() % (1025-w)), y = static_cast<int>(random() % (513-h));
        std::vector<std::uint8_t> encoded(static_cast<std::size_t>(w*h)*2);
        for (int j = 0; j < w*h; ++j) {
            const auto absolute = static_cast<std::size_t>(y + j/w)*1024 + static_cast<std::size_t>(x + j%w);
            const auto word = static_cast<std::uint16_t>(absolute ^ (n*317u));
            put16(encoded.data()+2*j, word); expected[absolute] = word; marked[absolute] = true;
        }
        auto view = data(allocations, encoded);
        auto memory = describe(allocations, view);
        const Nba97GameImageRect rect{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y),static_cast<std::int16_t>(w),static_cast<std::int16_t>(h)};
        check(vram.upload(rect,{&memory,0}) == R::Complete, "rectangular scatter upload");
        std::fill(encoded.begin(),encoded.end(),std::uint8_t{0});
        Nba97GameImageMemory read{encoded.data(),nullptr,encoded.size(),0,1};
        check(vram.store(rect,{&read,0}) == R::Complete, "rectangular readback");
        check(std::equal(encoded.begin(),encoded.end(),view.data), "all raw CPU words round trip");
    }
    for (std::size_t i = 0; i < expected.size(); ++i) if (marked[i])
        check(vram.word(i%1024,i/1024,value) && value == expected[i], "absolute-coordinate VRAM oracle");
}

void uploadAndStaging() {
    GameRenderBackend backend;
    // An 8-bit 6x2 image => 3 words/row, two946B8 tail uploads followed by a
    // temporary one-row main upload. The source restores height only at the end.
    std::vector<std::uint8_t> image(28);
    image[0] = 0x41; put16(image.data()+4,6); put16(image.data()+6,2);
    for (unsigned i=0;i<6;++i) put16(image.data()+16+i*2,static_cast<std::uint16_t>(0x100+i));
    auto view = data(backend.memory,image);
    Nba97GameRenderIoEvent event{};
    event.kind=NBA97_RENDER_UPLOAD_946B8; event.image={view,0}; event.x=10;event.y=20;
    auto staged = backend;
    check(staged.memory.rebind(backend.memory,event.image,event.image), "rebind staged image");
    check(GameRenderBackend::renderIo(&staged,&event)==0 && staged.lastResult==R::MaskModeUnknown,
        "upload requires actual mask-state provenance");
    check(backend.uploadState.pending_known==0 && view.data[0]==0x41, "failed candidate never changed live owner");
    staged = backend;
    event.image={view,0};
    check(staged.memory.rebind(backend.memory,event.image,event.image), "rebind second candidate");
    staged.unmaskedTransfersKnown=true; // explicit synthetic GPU-state fixture
    check(GameRenderBackend::renderIo(&staged,&event)==0 && staged.lastResult==R::SdkLimitsUnknown,
        "SDK clamp limits need original state provenance");
    staged.sdkTransferLimitsKnown=true;staged.sdkTransferWidth=1024;staged.sdkTransferHeight=512;
    check(GameRenderBackend::renderIo(&staged,&event)==1, "actual946B8 split owner composed with raw transfers");
    check(staged.uploadProgress.uploads_completed==3 && !staged.uploadProgress.temporary_height_active,
        "all three transfers and source height restoration");
    check(get16(event.image.storage.data+6)==2 && event.image.storage.data[0]==0x49,
        "source-mutated image retained");
    std::uint16_t value=0;
    for(unsigned i=0;i<6;++i)
        check(staged.vram.word(10+i%3,20+i/3,value)&&value==0x100+i,"split upload exact pixel arrangement");
    check(staged.uploadState.pending_known==1 && staged.uploadState.pending_d7b14==1,
        "944F4 writes pending after upload");
    Nba97GameRenderIoEvent sync{};sync.kind=NBA97_RENDER_SYNC_994F4;
    check(GameRenderBackend::renderIo(&staged,&sync)==1 && staged.uploadState.pending_d7b14==1,
        "994F4 sync does not clear D7B14");
    check(!backend.vram.word(10,20,value) && view.data[0]==0x41,"staged VRAM and allocation both isolated");
    backend=std::move(staged);
    check(backend.vram.word(12,21,value)&&value==0x105,"publish complete owned candidate");
    auto committed=backend.memory.buffer(1,0,28);
    check(committed.data[0]==0x49,"published source mutation");

    auto failing = backend;
    failing.sdkTransferWidth=1;
    Nba97GameRenderIoEvent limited{};limited.kind=NBA97_RENDER_UPLOAD_946B8;
    limited.image={failing.memory.buffer(1,0,28),0};limited.x=10;limited.y=20;
    check(GameRenderBackend::renderIo(&failing,&limited)==0 && failing.lastResult==R::SdkLimitsUnsupported,
        "rectangle needing source SDK clamp is outside supported domain");
    check(failing.uploadProgress.uploads_completed==1&&failing.vram.word(10,20,value)&&value==0x102,
        "first split tail remains when next transfer exceeds SDK limit");
    failing=backend;
    failing.sdkTransferWidth=-1;
    limited.image={failing.memory.buffer(1,0,28),0};
    check(GameRenderBackend::renderIo(&failing,&limited)==0&&failing.lastResult==R::SdkLimitsUnsupported&&
        failing.uploadProgress.uploads_completed==0,"SDK limits retain signed16 meaning");
    failing=backend;
    Nba97GameRenderIoEvent move{};move.kind=NBA97_RENDER_MOVE_997E4;move.rect={10,20,2,1};
    move.x=65536+30;move.y=65536+40;
    check(GameRenderBackend::renderIo(&failing,&move)==1&&failing.vram.word(30,40,value)&&value==0x100,
        "997E4 packs low16 destination coordinates");
    move.rect.w=0; failing.unmaskedTransfersKnown=false;
    check(GameRenderBackend::renderIo(&failing,&move)==1,"zero move returns before GPU dispatch");
    Nba97GameRenderIoEvent service{};service.kind=NBA97_RENDER_SERVICE_8892C;
    check(GameRenderBackend::renderIo(&failing,&service)==0&&failing.lastResult==R::ServiceRequired,
        "CD service is not a fake successful GPU sync");

    auto partial=backend;
    auto partialImage=partial.memory.buffer(1,0,28);
    put32(partialImage.data,0x00100041); // next header at+4096, after successful main upload
    Nba97GameRenderIoEvent partialEvent{};partialEvent.kind=NBA97_RENDER_UPLOAD_946B8;
    partialEvent.image={partialImage,0};partialEvent.x=100;partialEvent.y=100;
    check(GameRenderBackend::renderIo(&partial,&partialEvent)==0&&partial.lastUploadResult==NBA97_IMAGE_RESOURCE,
        "unowned next header refuses after transfer prefix");
    check(partial.uploadProgress.uploads_completed==3&&partial.uploadProgress.temporary_height_active&&
        get16(partialImage.data+6)==1,"original temporary height remains changed on later failure");
    check(partial.vram.word(100,100,value)&&value==0x100&&!backend.vram.word(100,100,value),
        "partial GPU effects live only in refused candidate");

    // Signed backward link to a palette in the SAME allocation. This is not a
    // detached image blob; allocation rebinding must retain preceding bytes.
    std::vector<std::uint8_t> backwards(84);
    backwards[0]=0x23;put16(backwards.data()+4,16);
    for(unsigned i=0;i<16;++i)put16(backwards.data()+16+2*i,static_cast<std::uint16_t>(0x700+i));
    put32(backwards.data()+64,0xffffc041u);put16(backwards.data()+68,4);put16(backwards.data()+70,1);
    put16(backwards.data()+80,0x4321);put16(backwards.data()+82,0x8765);
    auto backView=data(backend.memory,backwards);
    Nba97GameRenderIoEvent back{};back.kind=NBA97_RENDER_UPLOAD_946B8;back.image={backView,64};
    back.x=200;back.y=200;back.clut_x=512;back.clut_y=300;
    check(GameRenderBackend::renderIo(&backend,&back)==1&&backend.uploadProgress.headers_visited==2,
        "negative header link resolves through retained allocation");
    check(backend.vram.word(512+15,300,value)&&value==0x70f&&backend.vram.word(201,200,value)&&value==0x8765,
        "backward palette and image both transferred");
}

void malformedKnownness() {
    GameRenderBackend backend;
    std::vector<std::uint8_t> bytes(16);bytes[0]=0x23;
    put16(bytes.data()+4,2);put16(bytes.data()+12,0x1234);
    const auto id=backend.memory.add(bytes,std::vector<std::uint8_t>(16,1),0);
    auto view=backend.memory.buffer(id,0,16);
    auto memory=describe(backend.memory,view);
    memory.known[14]=2; // simulate invalid metadata introduced by a live callback
    Nba97GameRenderIoEvent event{};event.kind=NBA97_RENDER_UPLOAD_946B8;event.image={view,0};
    check(GameRenderBackend::renderIo(&backend,&event)==0&&backend.lastUploadResult==NBA97_IMAGE_ARGUMENT,
        "write-only source header field cannot erase malformed knownness");
    check(memory.known[14]==2&&get16(view.data+12)==0&&view.data[0]==0x23,
        "metadata refusal retains earlier x store and stops before later flag write");
}

struct ServiceFixture {
    unsigned calls=0;
    static int service(void* ctx,const Nba97GameRenderIoEvent* event) {
        auto& self=*static_cast<ServiceFixture*>(ctx);
        check(event->kind==NBA97_RENDER_SERVICE_8892C,"actual service event retained");
        ++self.calls;return 1; // explicit test boundary; not a production CD owner
    }
};
void composedHeadSwap() {
    GameRenderBackend backend; backend.unmaskedTransfersKnown=true;
    backend.sdkTransferLimitsKnown=true;backend.sdkTransferWidth=1024;backend.sdkTransferHeight=512;
    ServiceFixture service;backend.service=ServiceFixture::service;backend.serviceContext=&service;
    Nba97GameHeadCache head{};
    head.count[0]=2;head.current[0][0]=0;head.current[0][1]=1;head.lineup[0][0]=1;head.lineup[0][1]=0;
    head.xy[0][0]=20;head.xy[0][1]=40;head.xy[1][0]=80;head.xy[1][1]=40;
    // Valid one-record SHPP with image at18, pixels28, palette headerE68,
    // palette dataE78. Head cache's offsets and sizes are source-defined.
    std::vector<std::uint8_t> scratch(0x107c);
    put32(scratch.data(),0x50504853);put32(scratch.data()+8,1);put32(scratch.data()+20,24);
    put32(scratch.data()+24,(0xe50u<<8)|0x41u);put16(scratch.data()+28,76);put16(scratch.data()+30,48);
    scratch[0xe68]=0x23;put16(scratch.data()+0xe6c,256);put16(scratch.data()+0xe6e,1);
    head.scratch=data(backend.memory,scratch);
    for (unsigned slot=0;slot<2;++slot) {
        std::vector<std::uint8_t> pixels(38*48*2),palette(512);
        for(unsigned i=0;i<38*48;++i)put16(pixels.data()+2*i,static_cast<std::uint16_t>(1000+slot*2000+i));
        for(unsigned i=0;i<256;++i)put16(palette.data()+2*i,static_cast<std::uint16_t>(10000+slot*2000+i));
        auto p=data(backend.memory,pixels),pal=data(backend.memory,palette);
        auto pm=describe(backend.memory,p),cm=describe(backend.memory,pal);
        check(backend.vram.upload({static_cast<std::int16_t>(head.xy[slot][0]),40,38,48},{&pm,0})==R::Complete,"initial head fixture");
        check(backend.vram.upload({768,static_cast<std::int16_t>(246+slot),256,1},{&cm,0})==R::Complete,"initial palette fixture");
    }
    auto candidate=backend;auto candidateHead=head;
    check(candidate.memory.rebind(backend.memory,head.scratch,candidateHead.scratch),"stage scratch with VRAM");
    check(nba97_game_head_cache(&candidateHead,-1,GameRenderBackend::renderIo,&candidate)==NBA97_RENDER_COMPLETE,
        "38A18/3875C/946B8 actual readback+move+upload composition");
    check(service.calls==1 && candidateHead.current[0][0]==1&&candidateHead.current[0][1]==0,"source cache exchange and service");
    std::uint16_t value=0;
    for(unsigned slot=0;slot<2;++slot) {
        for(unsigned i=0;i<38*48;++i)
            check(candidate.vram.word(static_cast<std::size_t>(head.xy[slot][0])+i%38,40+i/38,value)&&
                value==1000+(1-slot)*2000+i,"every head pixel exchanged through actual VRAM");
        for(unsigned i=0;i<256;++i) {
            const auto expected=slot==0&&i==255?0x6af7u:10000+(1-slot)*2000+i;
            check(candidate.vram.word(768+i,246+slot,value)&&value==expected,"palette exchange retains original6AF7 overwrite");
        }
    }
    check(backend.vram.word(20,40,value)&&value==1000&&head.current[0][0]==0,"live cache and VRAM unchanged until publication");
    check(get16(candidateHead.scratch.data+0x1076)==0x6af7,"source scratch palette mutation retained");
}
}

int main() {
    try { memoryAndAliases();rawTransfers();uploadAndStaging();malformedKnownness();composedHeadSwap();
        std::cout<<"game_render_backend: "<<checks<<" checks passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
