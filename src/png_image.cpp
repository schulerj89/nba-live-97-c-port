#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>

#include "png_image.hpp"

#include <stdexcept>

namespace {
template <typename T> class ComPtr final {
public:
    ~ComPtr() { if (value_) value_->Release(); }
    T** put() noexcept { return &value_; }
    T* get() const noexcept { return value_; }
private:
    T* value_ = nullptr;
};

void check(HRESULT result, const char* operation) {
    if (FAILED(result)) throw std::runtime_error(std::string(operation) + " failed");
}
} // namespace

PshImage load_png_image(const std::filesystem::path& path) {
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(factory.put())), "WIC factory");
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory.get()->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, decoder.put()), "WIC PNG decoder");
    ComPtr<IWICBitmapFrameDecode> source;
    check(decoder.get()->GetFrame(0, source.put()), "WIC PNG frame");
    ComPtr<IWICFormatConverter> converter;
    check(factory.get()->CreateFormatConverter(converter.put()), "WIC converter");
    check(converter.get()->Initialize(source.get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom),
          "WIC RGBA conversion");
    UINT width = 0;
    UINT height = 0;
    check(converter.get()->GetSize(&width, &height), "WIC image size");
    if (!width || !height || width > 65535 || height > 65535)
        throw std::runtime_error("invalid PNG dimensions: " + path.string());
    PshImage image;
    image.source = path;
    image.tag = path.stem().string();
    image.width = static_cast<std::uint16_t>(width);
    image.height = static_cast<std::uint16_t>(height);
    image.rgba.resize(static_cast<std::size_t>(width) * height * 4);
    check(converter.get()->CopyPixels(nullptr, width * 4,
          static_cast<UINT>(image.rgba.size()), image.rgba.data()), "WIC pixel copy");
    return image;
}
