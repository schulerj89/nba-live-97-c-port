#include "ps1_vram_texture.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
PshImage image(int width, int height, std::array<std::uint8_t, 3> rgb) {
    PshImage out;
    out.width = width;
    out.height = height;
    out.rgba.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t at = 0; at < out.rgba.size(); at += 4) {
        out.rgba[at] = rgb[0]; out.rgba[at + 1] = rgb[1];
        out.rgba[at + 2] = rgb[2]; out.rgba[at + 3] = 255;
    }
    return out;
}
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        nba97::Ps1VramTextureAtlas atlas;
        atlas.upload8(image(255, 118, {{10, 20, 30}}), 832, 256);
        atlas.upload8(image(120, 80, {{40, 50, 60}}), 889, 374);
        atlas.upload4(image(64, 45, {{70, 80, 90}}), 922, 454);
        std::array<std::uint8_t, 3> rgb{};
        check(atlas.sample(0, 0x00BD, 10, 20, 0, 0, rgb) && rgb[0] == 10,
              "TPAGE BD jersey address");
        check(atlas.sample(0, 0x00BE, 0, 20, 0, 0, rgb) && rgb[1] == 20,
              "TPAGE BE crosses into jersey x=128");
        check(atlas.sample(0, 0x00BD, 114, 118, 0, 0, rgb) && rgb[2] == 60,
              "FUN_80067A14 shorts upload origin");
        check(atlas.sample(0, 0x003E, 104, 199, 0, 0, rgb) && rgb[0] == 70,
              "4-bpp SHOE upload origin");
        nba97::Ps1VramTextureAtlas indexed;
        indexed.upload8Indexed(2, 1, {0, 1}, 832, 256);
        indexed.uploadClut({0x9084, 0x001f}, 512, 500, 3);
        check(indexed.sample(0x7d20, 0x00bd, 0, 0, 3, 0, rgb) && rgb[0] == 33,
              "packet CLUT row resolves nonzero replacement color");
        check(indexed.sample(0x7d20, 0x00bd, 1, 0, 3, 0, rgb) && rgb[0] == 255,
              "packet CLUT row resolves indexed texel");
        nba97::Ps1VramTextureAtlas shared_body;
        std::vector<std::uint8_t> dthr(35 * 114, 0);
        dthr[44 * 35 + 30] = 234;
        std::vector<std::uint16_t> body_palette(256, 0x8000);
        body_palette[234] = 0xd71c;
        shared_body.upload8Indexed(35, 114, std::move(dthr), 832, 374);
        shared_body.uploadClut(std::move(body_palette), 512, 501, 9);
        nba97::Ps1TextureTrace body_trace{};
        check(shared_body.sampleDetailed(0x7d60, 0x00bd, 30, 162, 9, 0,
                                         rgb, &body_trace) ==
                  nba97::Ps1TextureSample::Opaque &&
                  body_trace.word_x == 847 && body_trace.word_y == 418 &&
                  body_trace.upload_x == 30 && body_trace.upload_y == 44 &&
                  body_trace.palette_index == 234 &&
                  body_trace.palette_value == 0xd71c && rgb[0] == 231 &&
                  rgb[1] == 198 && rgb[2] == 173,
              "FUN_80067F74 dthr face-1 sample resolves exact runtime CLUT color");
        nba97::Ps1VramTextureAtlas variants;
        variants.upload8Indexed(1, 1, {0}, 832, 256, 511);
        variants.uploadClut({0x03e0}, 512, 500, 300);
        check(variants.sample(0x7d20, 0x00bd, 0, 0, 300, 511, rgb) && rgb[1] == 255,
              "head and palette variant ids retain more than eight bits");
        nba97::Ps1VramTextureAtlas shared_clut;
        shared_clut.upload4Indexed(1, 1, {0}, 960, 256);
        shared_clut.uploadClut({0x001f}, 528, 502, 0xffff);
        check(shared_clut.sample(0x7da1, 0x003f, 0, 0, 47, 0, rgb) && rgb[0] == 255,
              "variant-independent jersey-number CLUT resolves every player palette");
        nba97::Ps1VramTextureAtlas overlap;
        overlap.upload8(image(2, 1, {{11, 22, 33}}), 832, 256);
        overlap.upload8(image(1, 1, {{99, 88, 77}}), 832, 256);
        check(overlap.sample(0, 0x00bd, 0, 0, 0, 0, rgb) && rgb[0] == 99,
              "later VRAM upload replaces an overlapping earlier upload");
        nba97::Ps1VramTextureAtlas cross_depth;
        cross_depth.upload8Indexed(2, 1, {0x12, 0x34}, 832, 256);
        cross_depth.upload4Indexed(4, 1, {1, 2, 3, 4}, 832, 256);
        std::vector<std::uint16_t> cross_palette(256, 0x8000);
        cross_palette[0x21] = 0x001f;
        cross_palette[0x43] = 0x03e0;
        cross_depth.uploadClut(std::move(cross_palette), 512, 500);
        check(cross_depth.sampleDetailed(0x7d20,0x00bd,0,0,0,0,rgb)==
                  nba97::Ps1TextureSample::Opaque && rgb[0]==255,
              "later 4-bpp upload replaces raw word sampled through 8-bpp TPAGE byte 0");
        check(cross_depth.sampleDetailed(0x7d20,0x00bd,1,0,0,0,rgb)==
                  nba97::Ps1TextureSample::Opaque && rgb[1]==255,
              "later 4-bpp upload replaces raw word sampled through 8-bpp TPAGE byte 1");
        nba97::Ps1VramTextureAtlas transparency;
        transparency.upload4Indexed(2, 1, {15, 0}, 832, 256);
        transparency.uploadClut({0x8000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},512,500);
        check(transparency.sampleDetailed(0x7d20,0x001d,0,0,0,0,rgb)==
                  nba97::Ps1TextureSample::Transparent,
              "zero CLUT entry is transparent");
        check(transparency.sampleDetailed(0x7d20,0x001d,1,0,0,0,rgb)==
                  nba97::Ps1TextureSample::Opaque && rgb[0]==0 && rgb[1]==0 && rgb[2]==0,
              "0x8000 CLUT entry is opaque black");
        nba97::Ps1VramTextureAtlas cache_invalidation;
        cache_invalidation.upload8Indexed(2,1,{0,0},832,256,7);
        cache_invalidation.uploadClut({0x001f,0x03e0},512,500,5);
        check(cache_invalidation.sample(0x7d20,0x00bd,0,0,5,7,rgb)&&rgb[0]==255,
              "variant candidate cache initial sample");
        cache_invalidation.upload8Indexed(2,1,{1,1},832,256,7);
        check(cache_invalidation.sample(0x7d20,0x00bd,0,0,5,7,rgb)&&rgb[1]==255,
              "upload candidate cache invalidates on later write");
        cache_invalidation.uploadClut({0x7c00,0x001f},512,500,5);
        check(cache_invalidation.sample(0x7d20,0x00bd,0,0,5,7,rgb)&&rgb[0]==255&&rgb[1]==0,
              "CLUT candidate cache invalidates on later write");

        nba97::Ps1VramTextureAtlas wildcard_after_exact;
        wildcard_after_exact.upload8Indexed(1,1,{0},832,256,7);
        wildcard_after_exact.upload8Indexed(1,1,{1},832,256,0xffff);
        wildcard_after_exact.upload8Indexed(1,1,{2},832,256,9);
        wildcard_after_exact.uploadClut({0x001f,0x03e0,0x7c00},512,500);
        check(wildcard_after_exact.sample(0x7d20,0x00bd,0,0,0,7,rgb)&&rgb[1]==255,
              "later wildcard texture upload wins over an earlier exact variant");

        nba97::Ps1VramTextureAtlas exact_after_wildcard;
        exact_after_wildcard.upload8Indexed(1,1,{0},832,256,0xffff);
        exact_after_wildcard.upload8Indexed(1,1,{1},832,256,7);
        exact_after_wildcard.upload8Indexed(1,1,{2},832,256,9);
        exact_after_wildcard.uploadClut({0x001f,0x03e0,0x7c00},512,500);
        check(exact_after_wildcard.sample(0x7d20,0x00bd,0,0,0,7,rgb)&&rgb[1]==255,
              "later exact texture variant wins over an earlier wildcard");
        check(exact_after_wildcard.sample(0x7d20,0x00bd,0,0,0,8,rgb)&&rgb[0]==255,
              "unrelated texture variants are skipped while wildcard remains active");

        nba97::Ps1VramTextureAtlas clut_precedence;
        clut_precedence.upload8Indexed(1,1,{0},832,256);
        clut_precedence.uploadClut({0x001f},512,500,5);
        clut_precedence.uploadClut({0x03e0},512,500,0xffff);
        clut_precedence.uploadClut({0x7c00},512,500,9);
        check(clut_precedence.sample(0x7d20,0x00bd,0,0,5,0,rgb)&&rgb[1]==255,
              "later wildcard CLUT wins over an earlier exact variant");

        nba97::Ps1VramTextureAtlas exact_clut_precedence;
        exact_clut_precedence.upload8Indexed(1,1,{0},832,256);
        exact_clut_precedence.uploadClut({0x001f},512,500,0xffff);
        exact_clut_precedence.uploadClut({0x03e0},512,500,5);
        exact_clut_precedence.uploadClut({0x7c00},512,500,9);
        check(exact_clut_precedence.sample(0x7d20,0x00bd,0,0,5,0,rgb)&&rgb[1]==255,
              "later exact CLUT variant wins over an earlier wildcard");
        check(exact_clut_precedence.sample(0x7d20,0x00bd,0,0,8,0,rgb)&&rgb[0]==255,
              "unrelated CLUT variants are skipped while wildcard remains active");
        std::cout << "PS1 VRAM TEXTURE: PASS - raw-word 4/8-bpp addressing, shared dthr face sample, packet CLUT selection, cached variant lookup, same/cross-depth last-write-wins overlap, and transparency\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PS1 VRAM TEXTURE: FAIL - " << error.what() << '\n';
        return 1;
    }
}
