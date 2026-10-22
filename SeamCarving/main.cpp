// main.cpp
// Assignment 2, Question 1: Seam carving with dynamic programming.
//
// Usage
//   Interactive (just press F5 in Visual Studio):
//       SeamCarving.exe
//   Command line:
//       SeamCarving.exe <input> <output> <targetWidth> <targetHeight>
//       SeamCarving.exe --test
//
// For an output named result.png the program also writes:
//   result_energy.png  - the initial Sobel energy map
//   result_seams.png   - the original image with every removed seam drawn
//                        (red = vertical, blue = horizontal)
#include "Image.h"
#include "SeamCarver.h"
#include "Tests.h"

#include <cstdio>
#include <exception>
#include <iostream>
#include <string>

namespace
{
    std::string trim(const std::string& s)
    {
        const size_t a = s.find_first_not_of(" \t\r\n\"");
        if (a == std::string::npos) return "";
        const size_t b = s.find_last_not_of(" \t\r\n\"");
        return s.substr(a, b - a + 1);
    }

    // Reads a line; returns `fallback` if the user just presses Enter.
    std::string ask(const std::string& prompt, const std::string& fallback)
    {
        std::cout << prompt;
        if (!fallback.empty()) std::cout << " [" << fallback << "]";
        std::cout << ": ";
        std::string line;
        std::getline(std::cin, line);
        line = trim(line);
        return line.empty() ? fallback : line;
    }

    int askInt(const std::string& prompt, int fallback, int lo, int hi)
    {
        while (true)
        {
            const std::string s = ask(prompt + " (" + std::to_string(lo) + "-" + std::to_string(hi) + ")",
                                      std::to_string(fallback));
            try
            {
                size_t used = 0;
                const int v = std::stoi(s, &used);
                if (used == s.size() && v >= lo && v <= hi) return v;
            }
            catch (...) {}
            std::cout << "  Please enter a whole number between " << lo << " and " << hi << ".\n";
        }
    }

    // "out/result.png" + "_seams" -> "out/result_seams.png"
    std::string withSuffix(const std::string& path, const std::string& suffix)
    {
        const size_t slash = path.find_last_of("/\\");
        const size_t dot = path.find_last_of('.');
        if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
            return path + suffix + ".png";
        return path.substr(0, dot) + suffix + ".png";
    }

    int resizeImage(const std::string& inputPath, const std::string& outputPath, int targetW, int targetH)
    {
        Image input;
        std::string error;
        if (!loadImage(inputPath, input, error))
        {
            std::cerr << "Could not open '" << inputPath << "': " << error << "\n";
            return 1;
        }

        std::cout << "\nInput : " << inputPath << " (" << input.width << " x " << input.height << ")\n";
        std::cout << "Target: " << targetW << " x " << targetH << "\n";
        std::cout << "Carving...\n";

        sc::CarveResult result;
        try
        {
            result = sc::carve(input, targetW, targetH);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Error: " << ex.what() << "\n";
            return 1;
        }

        const std::string energyPath = withSuffix(outputPath, "_energy");
        const std::string seamsPath = withSuffix(outputPath, "_seams");

        bool ok = saveImage(outputPath, result.output);
        ok &= saveImage(energyPath, sc::energyToImage(sc::computeEnergy(input), input.width, input.height));
        ok &= saveImage(seamsPath, result.seamMap);
        if (!ok)
        {
            std::cerr << "Failed to write one of the output files.\n";
            return 1;
        }

        const sc::CarveStats& s = result.stats;
        std::printf("\nDone: %d x %d -> %d x %d\n", input.width, input.height,
                    result.output.width, result.output.height);
        std::printf("  Vertical seams removed   : %d\n", s.verticalSeamsRemoved);
        std::printf("  Horizontal seams removed : %d\n", s.horizontalSeamsRemoved);
        std::printf("  Time - energy maps       : %10.2f ms\n", s.energyMs);
        std::printf("  Time - DP seam search    : %10.2f ms\n", s.seamMs);
        std::printf("  Time - seam removal      : %10.2f ms\n", s.removeMs);
        std::printf("  Time - total             : %10.2f ms\n", s.totalMs);
        std::cout << "\nWrote:\n  " << outputPath << "\n  " << energyPath << "\n  " << seamsPath << "\n";
        return 0;
    }

    int interactive()
    {
        std::cout << "=========================================\n";
        std::cout << " Seam Carving - Dynamic Programming (Q1)\n";
        std::cout << "=========================================\n";
        std::cout << " 1. Resize an image\n";
        std::cout << " 2. Run correctness tests\n";
        const std::string choice = ask("Choice", "1");

        if (choice == "2")
            return runAllTests() ? 0 : 1;

        const std::string inputPath = ask("Input image path", "sample.png");

        Image probe;
        std::string error;
        if (!loadImage(inputPath, probe, error))
        {
            std::cerr << "Could not open '" << inputPath << "': " << error << "\n"
                      << "Tip: put the image next to the .vcxproj file, or type a full path.\n";
            return 1;
        }
        std::cout << "Image is " << probe.width << " x " << probe.height << "\n";

        const int targetW = askInt("Target width", probe.width * 3 / 4 > 0 ? probe.width * 3 / 4 : 1, 1, probe.width);
        const int targetH = askInt("Target height", probe.height, 1, probe.height);
        const std::string outputPath = ask("Output path", "output.png");

        return resizeImage(inputPath, outputPath, targetW, targetH);
    }
}

int main(int argc, char* argv[])
{
    int code = 0;
    if (argc == 2 && std::string(argv[1]) == "--test")
    {
        return runAllTests() ? 0 : 1;
    }
    if (argc == 5)
    {
        try
        {
            return resizeImage(argv[1], argv[2], std::stoi(argv[3]), std::stoi(argv[4]));
        }
        catch (const std::exception&)
        {
            std::cerr << "Width and height must be whole numbers.\n";
            return 1;
        }
    }
    if (argc != 1)
    {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << "                                   (interactive)\n"
                  << "  " << argv[0] << " <input> <output> <width> <height>\n"
                  << "  " << argv[0] << " --test\n";
        return 1;
    }

    code = interactive();
    std::cout << "\nPress Enter to exit...";
    std::string dummy;
    std::getline(std::cin, dummy);
    return code;
}
