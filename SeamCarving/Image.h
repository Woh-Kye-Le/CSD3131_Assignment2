// Image.h
// Minimal RGB image container plus load/save helpers (backed by stb_image).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Pixel
{
    uint8_t r = 0, g = 0, b = 0;
};

struct Image
{
    int width = 0;
    int height = 0;
    std::vector<Pixel> pixels; // row-major: index = y * width + x

    Image() = default;
    Image(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * h) {}

    Pixel& at(int x, int y) { return pixels[static_cast<size_t>(y) * width + x]; }
    const Pixel& at(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }
};

// Loads any format stb_image supports (PNG, JPG, BMP, TGA, ...). Alpha is dropped.
bool loadImage(const std::string& path, Image& out, std::string& error);

// Saves as JPG if the path ends in .jpg/.jpeg, BMP if .bmp, otherwise PNG.
bool saveImage(const std::string& path, const Image& img);
