// ImageIO.cpp
// Image reading/writing only. The assignment allows third-party libraries for
// image I/O; all seam-carving logic is implemented by hand in SeamCarver.cpp.
#define _CRT_SECURE_NO_WARNINGS

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "Image.h"

#include <algorithm>
#include <cctype>

bool loadImage(const std::string& path, Image& out, std::string& error)
{
    int w = 0, h = 0, channels = 0;
    // Ask stb for 3 channels so every image arrives as RGB.
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 3);
    if (!data)
    {
        error = stbi_failure_reason() ? stbi_failure_reason() : "unknown error";
        return false;
    }

    out = Image(w, h);
    for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i)
    {
        out.pixels[i].r = data[i * 3 + 0];
        out.pixels[i].g = data[i * 3 + 1];
        out.pixels[i].b = data[i * 3 + 2];
    }
    stbi_image_free(data);
    return true;
}

static std::string lowerExtension(const std::string& path)
{
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

bool saveImage(const std::string& path, const Image& img)
{
    // Pixel is three tightly packed uint8_t, so the buffer is already RGB24.
    std::vector<unsigned char> buf(static_cast<size_t>(img.width) * img.height * 3);
    for (size_t i = 0; i < img.pixels.size(); ++i)
    {
        buf[i * 3 + 0] = img.pixels[i].r;
        buf[i * 3 + 1] = img.pixels[i].g;
        buf[i * 3 + 2] = img.pixels[i].b;
    }

    const std::string ext = lowerExtension(path);
    int ok = 0;
    if (ext == ".jpg" || ext == ".jpeg")
        ok = stbi_write_jpg(path.c_str(), img.width, img.height, 3, buf.data(), 95);
    else if (ext == ".bmp")
        ok = stbi_write_bmp(path.c_str(), img.width, img.height, 3, buf.data());
    else
        ok = stbi_write_png(path.c_str(), img.width, img.height, 3, buf.data(), img.width * 3);
    return ok != 0;
}
