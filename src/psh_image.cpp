#include "psh_image.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
std::uint16_t read_u16(const std::vector<std::uint8_t>& data, std::size_t at) {
    if (at + 2 > data.size()) throw std::runtime_error("truncated PSH u16");
    return static_cast<std::uint16_t>(data[at] | (data[at + 1] << 8));
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& data, std::size_t at) {
    if (at + 4 > data.size()) throw std::runtime_error("truncated PSH u32");
    return static_cast<std::uint32_t>(data[at]) |
           (static_cast<std::uint32_t>(data[at + 1]) << 8) |
           (static_cast<std::uint32_t>(data[at + 2]) << 16) |
           (static_cast<std::uint32_t>(data[at + 3]) << 24);
}

bool magic_at(const std::vector<std::uint8_t>& data, std::size_t at,
              const char* magic) {
    return at + 4 <= data.size() && data[at] == magic[0] &&
           data[at + 1] == magic[1] && data[at + 2] == magic[2] &&
           data[at + 3] == magic[3];
}

std::uint8_t expand5(std::uint16_t value) {
    return static_cast<std::uint8_t>((value << 3) | (value >> 2));
}

void write_bgr555(std::vector<std::uint8_t>& rgba, std::size_t pixel,
                  std::uint16_t color) {
    const std::size_t out = pixel * 4;
    rgba[out] = expand5(color & 0x1f);
    rgba[out + 1] = expand5((color >> 5) & 0x1f);
    rgba[out + 2] = expand5((color >> 10) & 0x1f);
    // In PS1 texture data, zero is transparent. STP is a blend-mode bit,
    // not ordinary host alpha, so non-zero pixels remain visible here.
    rgba[out + 3] = color == 0 ? 0 : 255;
}
} // namespace

PshImage load_psh(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open " + path.string());
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
    if (data.size() < 52 || !magic_at(data, 0, "SHPP") ||
        !magic_at(data, 12, "GIMX")) {
        throw std::runtime_error(path.string() + ": not an SHPP/GIMX image");
    }

    PshImage image;
    image.source = path;
    image.tag.assign(reinterpret_cast<const char*>(data.data() + 16), 4);
    image.format = read_u32(data, 24);
    image.width = read_u16(data, 28);
    image.height = read_u16(data, 30);
    image.has_crcf = magic_at(data, data.size() - 12, "CRCF");
    if (!image.width || !image.height || !image.has_crcf) {
        throw std::runtime_error(path.string() + ": invalid dimensions or CRCF trailer");
    }

    const std::size_t pixels = static_cast<std::size_t>(image.width) * image.height;
    image.rgba.resize(pixels * 4);
    if ((image.format & 0xff) == 0x42) {
        if (40 + pixels * 2 > data.size() - 12) {
            throw std::runtime_error(path.string() + ": truncated 16-bit pixels");
        }
        for (std::size_t i = 0; i < pixels; ++i) {
            write_bgr555(image.rgba, i, read_u16(data, 40 + i * 2));
        }
    } else if ((image.format & 0xff) == 0x41) {
        // NBA Live 97 stores 8-bit indices first, then a 16-byte palette
        // descriptor and 256 BGR555 colors.
        const std::size_t palette = 40 + pixels + 16;
        if (palette + 512 > data.size() - 12) {
            throw std::runtime_error(path.string() + ": truncated 8-bit palette");
        }
        for (std::size_t i = 0; i < pixels; ++i) {
            write_bgr555(image.rgba, i, read_u16(data, palette + data[40 + i] * 2));
        }
    } else {
        std::ostringstream error;
        error << path.string() << ": unsupported PSH format 0x" << std::hex
              << image.format;
        throw std::runtime_error(error.str());
    }
    return image;
}

std::string describe_psh(const PshImage& image) {
    std::ostringstream out;
    out << image.source.string() << " tag=" << image.tag << " format=0x"
        << std::hex << image.format << std::dec << " " << image.width << 'x'
        << image.height << " CRCF=" << (image.has_crcf ? "yes" : "no");
    return out.str();
}
