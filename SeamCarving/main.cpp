// main.cpp
// Assignment 2: seam carving with dynamic programming and greedy comparison.
//
// Usage
//   Interactive (just press F5 in Visual Studio):
//       SeamCarving.exe
//       1. Resize an image      2. Run correctness tests
//       3. Compare DP and greedy 4. Exit
//   Command line:
//       SeamCarving.exe <input> <width> <height>
//       SeamCarving.exe <input> <width> <height> greedy
//       SeamCarving.exe --compare <input> <width> <height>
//       SeamCarving.exe --test
//
// Input images live in input/ and every generated image goes into output/,
// both next to this file. The output is named after the input and the
// algorithm, so input/astronaut.png with dp gives:
//   output_astronaut_dp.png        - the resized image
//   output_energy_astronaut_dp.png - the initial Sobel energy map
//   output_seams_astronaut_dp.png  - the original image with every removed
//                                    seam drawn (red = vertical, blue = horizontal)
//   compare mode writes the same three files for both dp and greedy, plus
//   output_astronaut_comparison.png.
#include <algorithm>
#include <cctype>
#include <cmath>
#include "Image.h"
#include "SeamCarver.h"
#include "Tests.h"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    struct AlgorithmChoice
    {
        std::string name;
        sc::SeamFinder finder;
    };

    struct BenchResult
    {
        sc::CarveResult result;
        sc::CarveStats bestStats;
    };

    std::string trim(const std::string& s)
    {
        const size_t a = s.find_first_not_of(" \t\r\n\"");
        if (a == std::string::npos) return "";
        const size_t b = s.find_last_not_of(" \t\r\n\"");
        return s.substr(a, b - a + 1);
    }

    std::string lower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // Reads a line; returns `fallback` if the user just presses Enter.
    // `showDefault` controls whether the fallback is echoed in the prompt.
    std::string ask(const std::string& prompt, const std::string& fallback, bool showDefault = true)
    {
        std::cout << prompt;
        if (showDefault && !fallback.empty()) std::cout << " [" << fallback << "]";
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
                                      std::to_string(fallback), false);
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

    // Every generated image lands in this folder.
    const std::string kOutputDir = "output/";
    const std::string kInputDir = "input/";

    // Prints the image files currently sitting in input/.
    void printInputImages()
    {
        std::vector<std::string> names;
        std::error_code ec;
        const fs::path inputDir(kInputDir);
        if (!fs::exists(inputDir, ec) || !fs::is_directory(inputDir, ec))
            return;

        for (const fs::directory_entry& entry : fs::directory_iterator(inputDir, ec))
        {
            if (ec) break;
            if (!entry.is_regular_file(ec)) continue;

            const std::string ext = lower(entry.path().extension().string());
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
                names.push_back(entry.path().filename().string());
        }

        if (names.empty()) return;

        std::sort(names.begin(), names.end());
        std::cout << "Input images in " << kInputDir << ":\n";
        for (const std::string& name : names)
            std::cout << "  - " << name << "\n";
    }

    // "input/astronaut.png" -> "astronaut"
    std::string originalName(const std::string& path)
    {
        const size_t slash = path.find_last_of("/\\");
        const size_t start = (slash == std::string::npos) ? 0 : slash + 1;
        const size_t dot = path.find_last_of('.');
        if (dot == std::string::npos || dot < start) return path.substr(start);
        return path.substr(start, dot - start);
    }

    // Builds "output/output_<kind><name>_<algo>.png" from the input image name.
    // e.g. outputName("astronaut", "dp", "")       -> "output/output_astronaut_dp.png"
    //      outputName("astronaut", "dp", "_seams") -> "output/output_seams_astronaut_dp.png"
    std::string outputName(const std::string& name, const std::string& algo, const std::string& kind)
    {
        return kOutputDir + "output" + kind + "_" + name + "_" + algo + ".png";
    }

    // Parses the optional algorithm name from the CLI / menu.
    AlgorithmChoice parseAlgorithm(const std::string& name)
    {
        const std::string key = lower(trim(name));

        if (key.empty() ||
            key == "dp" ||
            key == "dynamic" ||
            key == "dynamic-programming")
        {
            return { "dp", sc::findVerticalSeamDP };
        }

        if (key == "greedy")
            return { "greedy", sc::findVerticalSeamGreedy };

        if (key == "graphcut" ||
            key == "graph-cut" ||
            key == "cut")
        {
            return { "graphcut", sc::findVerticalSeamGraphCut };
        }

        throw std::invalid_argument(
            "Algorithm must be 'dp', 'greedy', or 'graphcut'."
        );
    }

    void blit(Image& dst, int dstX, int dstY, const Image& src)
    {
        for (int y = 0; y < src.height; ++y)
            for (int x = 0; x < src.width; ++x)
                dst.at(dstX + x, dstY + y) = src.at(x, y);
    }

    // White = both, cyan = DP only, orange = greedy only.
    Image seamDifferenceImage(const std::vector<uint8_t>& dp, const std::vector<uint8_t>& greedy, int width, int height)
    {
        Image out(width, height);
        for (size_t i = 0; i < dp.size(); ++i)
        {
            const bool inDp = dp[i] != 0;
            const bool inGreedy = greedy[i] != 0;
            if (inDp && inGreedy) out.pixels[i] = { 255, 255, 255 };
            else if (inDp)        out.pixels[i] = { 0, 255, 255 };
            else if (inGreedy)    out.pixels[i] = { 255, 165, 0 };
            else                  out.pixels[i] = { 0, 0, 0 };
        }
        return out;
    }

    // 2x3 sheet:
    //   top    = DP seams | greedy seams | seam difference
    //   bottom = original | DP result    | greedy result
    Image makeComparisonSheet(const Image& original, const sc::CarveResult& dpResult, const sc::CarveResult& greedyResult)
    {
        const Image diff = seamDifferenceImage(dpResult.removedBy, greedyResult.removedBy, original.width, original.height);
        const int cellW = std::max(original.width, std::max(dpResult.output.width, greedyResult.output.width));
        const int cellH = std::max(original.height, std::max(dpResult.output.height, greedyResult.output.height));

        Image sheet(cellW * 3, cellH * 2);
        blit(sheet, 0 * cellW, 0 * cellH, dpResult.seamMap);
        blit(sheet, 1 * cellW, 0 * cellH, greedyResult.seamMap);
        blit(sheet, 2 * cellW, 0 * cellH, diff);
        blit(sheet, 0 * cellW, 1 * cellH, original);
        blit(sheet, 1 * cellW, 1 * cellH, dpResult.output);
        blit(sheet, 2 * cellW, 1 * cellH, greedyResult.output);
        return sheet;
    }

    // Percentage by which `value` exceeds `baseline`.
    double percentExtra(double value, double baseline)
    {
        if (baseline == 0.0) return value == 0.0 ? 0.0 : std::numeric_limits<double>::infinity();
        return (value - baseline) * 100.0 / baseline;
    }

    // How many times faster `faster` is than `slower`.
    double speedup(double slower, double faster)
    {
        if (faster <= 0.0) return std::numeric_limits<double>::infinity();
        return slower / faster;
    }

    // Of the pixels removed by A, what fraction are also removed by B?
    double removedPixelShare(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
    {
        size_t shared = 0;
        size_t removed = 0;
        for (size_t i = 0; i < a.size(); ++i)
        {
            const bool inA = a[i] != 0;
            const bool inB = b[i] != 0;
            if (inA && inB) ++shared;
            if (inA) ++removed;
        }
        return removed == 0 ? 100.0 : 100.0 * static_cast<double>(shared) / static_cast<double>(removed);
    }

    // PSNR between two same-sized RGB images.
    double computePSNR(const Image& a, const Image& b)
    {
        if (a.width != b.width || a.height != b.height) return 0.0;
        double sse = 0.0;
        for (size_t i = 0; i < a.pixels.size(); ++i)
        {
            const int dr = static_cast<int>(a.pixels[i].r) - static_cast<int>(b.pixels[i].r);
            const int dg = static_cast<int>(a.pixels[i].g) - static_cast<int>(b.pixels[i].g);
            const int db = static_cast<int>(a.pixels[i].b) - static_cast<int>(b.pixels[i].b);
            sse += static_cast<double>(dr * dr + dg * dg + db * db);
        }
        if (sse == 0.0) return std::numeric_limits<double>::infinity();
        const double mse = sse / (static_cast<double>(a.pixels.size()) * 3.0);
        return 10.0 * std::log10((255.0 * 255.0) / mse);
    }

    // Prints one timing / energy summary block.
    void printStats(const std::string& label, const sc::CarveStats& s)
    {
        std::printf("\n[%s]\n", label.c_str());
        std::printf("  Vertical seams removed   : %d\n", s.verticalSeamsRemoved);
        std::printf("  Horizontal seams removed : %d\n", s.horizontalSeamsRemoved);
        std::printf("  Removed energy           : %10.2f\n", s.removedEnergy);
        std::printf("  Time - energy maps       : %10.2f ms\n", s.energyMs);
        std::printf("  Time - seam search       : %10.2f ms\n", s.seamMs);
        std::printf("  Time - seam removal      : %10.2f ms\n", s.removeMs);
        std::printf("  Time - total             : %10.2f ms\n", s.totalMs);
    }

    int loadInputImage(const std::string& inputPath, Image& input)
    {
        std::string error;
        if (!loadImage(inputPath, input, error))
        {
            std::cerr << "Could not open '" << inputPath << "': " << error << "\n";
            return 1;
        }
        return 0;
    }

    // Runs the same carve three times and keeps the best timings.
    BenchResult carveBestOf3(const Image& input, int targetW, int targetH, const sc::SeamFinder& finder)
    {
        BenchResult bench;
        bench.bestStats.energyMs = std::numeric_limits<double>::infinity();
        bench.bestStats.seamMs = std::numeric_limits<double>::infinity();
        bench.bestStats.removeMs = std::numeric_limits<double>::infinity();
        bench.bestStats.totalMs = std::numeric_limits<double>::infinity();
        bench.bestStats.removedEnergy = std::numeric_limits<double>::infinity();

        bool first = true;
        for (int i = 0; i < 3; ++i)
        {
            sc::CarveResult run = sc::carve(input, targetW, targetH, finder);
            if (first)
            {
                bench.result = run;
                bench.bestStats = run.stats;
                first = false;
                continue;
            }

            bench.bestStats.verticalSeamsRemoved = run.stats.verticalSeamsRemoved;
            bench.bestStats.horizontalSeamsRemoved = run.stats.horizontalSeamsRemoved;
            bench.bestStats.removedEnergy = std::min(bench.bestStats.removedEnergy, run.stats.removedEnergy);
            bench.bestStats.energyMs = std::min(bench.bestStats.energyMs, run.stats.energyMs);
            bench.bestStats.seamMs = std::min(bench.bestStats.seamMs, run.stats.seamMs);
            bench.bestStats.removeMs = std::min(bench.bestStats.removeMs, run.stats.removeMs);
            bench.bestStats.totalMs = std::min(bench.bestStats.totalMs, run.stats.totalMs);
        }
        return bench;
    }

    // Runs one resize using the chosen seam finder. Outputs go to output/.
    int resizeImage(const std::string& inputPath, int targetW, int targetH, const AlgorithmChoice& algorithm)
    {
        Image input;
        if (loadInputImage(inputPath, input) != 0) return 1;

        const std::string name = originalName(inputPath);
        const std::string outputPath = outputName(name, algorithm.name, "");
        const std::string energyPath = outputName(name, algorithm.name, "_energy");
        const std::string seamsPath = outputName(name, algorithm.name, "_seams");

        std::cout << "\nInput    : " << inputPath << " (" << input.width << " x " << input.height << ")\n";
        std::cout << "Target   : " << targetW << " x " << targetH << "\n";
        std::cout << "Algorithm: " << algorithm.name << "\n";
        std::cout << "Carving...\n";

        sc::CarveResult result;
        try
        {
            result = sc::carve(input, targetW, targetH, algorithm.finder);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Error: " << ex.what() << "\n";
            return 1;
        }

        bool ok = saveImage(outputPath, result.output);
        ok &= saveImage(energyPath, sc::energyToImage(sc::computeEnergy(input), input.width, input.height));
        ok &= saveImage(seamsPath, result.seamMap);
        if (!ok)
        {
            std::cerr << "Failed to write one of the output files.\n";
            return 1;
        }

        std::printf("\nDone: %d x %d -> %d x %d\n", input.width, input.height,
                    result.output.width, result.output.height);
        printStats(algorithm.name, result.stats);
        std::cout << "\nWrote:\n  " << outputPath << "\n  " << energyPath << "\n  " << seamsPath << "\n";
        return 0;
    }

    // Runs both algorithms, saves their outputs, and prints the comparison.
    int compareAlgorithms(const std::string& inputPath, int targetW, int targetH, const AlgorithmChoice& alternative = { "greedy", sc::findVerticalSeamGreedy })
    {
        Image input;
        if (loadInputImage(inputPath, input) != 0) return 1;

        const std::string name = originalName(inputPath);
        const std::string dpPath = outputName(name, "dp", "");
        const std::string otherPath = outputName(name, alternative.name, "");
        const std::string dpSeamsPath = outputName(name, "dp", "_seams");
        const std::string otherSeamsPath = outputName(name, alternative.name, "_seams");
        const std::string dpEnergyPath = outputName(name, "dp", "_energy");
        const std::string otherEnergyPath = outputName(name, alternative.name, "_energy");
        const std::string sheetPath = outputName(name, alternative.name == "greedy" ? "comparison" : "comparison_graphcut", "");

        std::cout << "\nInput : " << inputPath << " (" << input.width << " x " << input.height << ")\n";
        std::cout << "Target: " << targetW << " x " << targetH << "\n";
        std::cout << "Running DP and " << alternative.name << "...\n";

        BenchResult dpBench;
        BenchResult otherBench;
        try
        {
            dpBench = carveBestOf3(input, targetW, targetH, sc::findVerticalSeamDP);
            otherBench = carveBestOf3(input, targetW, targetH, alternative.finder);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Error: " << ex.what() << "\n";
            return 1;
        }

        const sc::CarveResult& dpResult = dpBench.result;
        const sc::CarveResult& otherResult = otherBench.result;

        const Image sheet = makeComparisonSheet(input, dpResult, otherResult);
        const Image energy = sc::energyToImage(sc::computeEnergy(input), input.width, input.height);
        bool ok = saveImage(dpPath, dpResult.output);
        ok &= saveImage(otherPath, otherResult.output);
        ok &= saveImage(dpSeamsPath, dpResult.seamMap);
        ok &= saveImage(otherSeamsPath, otherResult.seamMap);
        ok &= saveImage(dpEnergyPath, energy);
        ok &= saveImage(otherEnergyPath, energy);
        ok &= saveImage(sheetPath, sheet);
        if (!ok)
        {
            std::cerr << "Failed to write one of the comparison output files.\n";
            return 1;
        }

        printStats("dp", dpBench.bestStats);
        printStats(alternative.name, otherBench.bestStats);

        std::cout << "\nComparing DP with " << alternative.name << "\n";
        std::printf("\n[comparison] (best of 3 runs per timing field)\n");
        std::printf("  Other extra removed energy : %10.2f %%\n",
            percentExtra(otherBench.bestStats.removedEnergy, dpBench.bestStats.removedEnergy));
        std::printf("  Shared removed pixels       : %10.2f %%\n",
            removedPixelShare(dpResult.removedBy, otherResult.removedBy));
        std::printf("  PSNR(dp, other)            : %10.2f dB\n",
            computePSNR(dpResult.output, otherResult.output));
        std::printf("  Seam-search speed-up        : %10.6f x\n",
            speedup(dpBench.bestStats.seamMs, otherBench.bestStats.seamMs));
        std::printf("  Total speed-up              : %10.6f x\n",
            speedup(dpBench.bestStats.totalMs, otherBench.bestStats.totalMs));

        std::cout << "\nWrote:\n"
            << "  " << dpPath << "\n"
            << "  " << otherPath << "\n"
            << "  " << dpSeamsPath << "\n"
            << "  " << otherSeamsPath << "\n"
            << "  " << dpEnergyPath << "\n"
            << "  " << otherEnergyPath << "\n"
            << "  " << sheetPath << "\n";
        return 0;
    }

    // Menu loop: keeps offering the actions until the user picks 4 (Exit).
    int interactive()
    {
        while (true)
        {
            std::cout << "\n=========================================\n";
            std::cout << " Seam Carving\n";
            std::cout << "=========================================\n";
            std::cout << " 1. Resize an image\n";
            std::cout << " 2. Run correctness tests\n";
            std::cout << " 3. Compare DP and greedy or graph cut\n";
            std::cout << " 4. Exit\n";
            const std::string choice = ask("Choice [1-4]", "1", false);

            if (choice == "4")
                return 0;
            if (choice == "2")
            {
                (void)runAllTests();
                continue;
            }
            if (choice != "1" && choice != "3")
            {
                std::cout << "  Please enter 1, 2, 3 or 4.\n";
                continue;
            }

            printInputImages();
            const std::string inputPath = ask("Input image path", "input/sample.png");

            Image probe;
            std::string error;
            if (!loadImage(inputPath, probe, error))
            {
                std::cerr << "Could not open '" << inputPath << "': " << error << "\n"
                          << "Tip: put the image in the input folder, or type a full path.\n";
                continue;
            }
            std::cout << "Image is " << probe.width << " x " << probe.height << "\n";

            const int targetW = askInt("Target width", probe.width * 3 / 4 > 0 ? probe.width * 3 / 4 : 1, 1, probe.width);
            const int targetH = askInt("Target height", probe.height, 1, probe.height);

            if (choice == "3")
            {
                const std::string other = ask(
                    "Compare DP with (greedy/graphcut)",
                    "graphcut",
                    false
                );

                try
                {
                    compareAlgorithms(
                        inputPath,
                        targetW,
                        targetH,
                        parseAlgorithm(other)
                    );
                }
                catch (const std::exception& ex)
                {
                    std::cerr << ex.what() << "\n";
                }

                continue;
            }

            AlgorithmChoice algorithm;
            try
            {
                algorithm = parseAlgorithm(ask("Algorithm (dp/greedy/graphcut)", "dp", false));
            }
            catch (const std::exception& ex)
            {
                std::cerr << ex.what() << "\n";
                continue;
            }
            resizeImage(inputPath, targetW, targetH, algorithm);
        }
    }
}

int main(int argc, char* argv[])
{
    int code = 0;
    if (argc == 2 && std::string(argv[1]) == "--test")
    {
        return runAllTests() ? 0 : 1;
    }
    if ((argc == 5 || argc == 6) && std::string(argv[1]) == "--compare")
    {
        try
        {
            return compareAlgorithms(argv[2], std::stoi(argv[3]), std::stoi(argv[4]),
                parseAlgorithm(argc == 6 ? argv[5] : "greedy"));
        }
        catch (const std::exception& ex)
        {
            std::cerr << ex.what() << "\n";
            return 1;
        }
    }
    if (argc == 4 || argc == 5)
    {
        try
        {
            const AlgorithmChoice algorithm = argc == 5 ? parseAlgorithm(argv[4]) : parseAlgorithm("dp");
            return resizeImage(argv[1], std::stoi(argv[2]), std::stoi(argv[3]), algorithm);
        }
        catch (const std::exception& ex)
        {
            std::cerr << ex.what() << "\n";
            return 1;
        }
    }
    if (argc != 1)
    {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << "                                   (interactive)\n"
            << "  " << argv[0]
            << " <input> <width> <height> [dp|greedy|graphcut]\n"
            << "  " << argv[0]
            << " --compare <input> <width> <height> [greedy|graphcut]\n"
                  << "  " << argv[0] << " --test\n";
        return 1;
    }

    code = interactive();
    std::cout << "\nPress Enter to exit...";
    std::string dummy;
    std::getline(std::cin, dummy);
    return code;
}
