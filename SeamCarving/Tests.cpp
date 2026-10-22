// Tests.cpp
// Correctness tests for the DP seam carver. The key test compares the DP
// answer against brute-force enumeration of every possible seam.
#include "Tests.h"
#include "SeamCarver.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>

using namespace sc;

namespace
{
    int g_passed = 0;
    int g_failed = 0;

    void check(bool condition, const std::string& name)
    {
        if (condition) { ++g_passed; std::cout << "  [PASS] " << name << "\n"; }
        else           { ++g_failed; std::cout << "  [FAIL] " << name << "\n"; }
    }

    // Exhaustively walks every connected seam from row `row`, column `col`.
    void enumerateSeams(const EnergyMap& e, int w, int h, int row, int col,
                        double acc, double& best, long long& count)
    {
        acc += e[static_cast<size_t>(row) * w + col];
        if (row == h - 1)
        {
            ++count;
            best = std::min(best, acc);
            return;
        }
        for (int d = -1; d <= 1; ++d)
        {
            const int next = col + d;
            if (next >= 0 && next < w)
                enumerateSeams(e, w, h, row + 1, next, acc, best, count);
        }
    }

    // Brute force: minimum seam cost over ALL seams, and how many seams exist.
    double bruteForceMinSeam(const EnergyMap& e, int w, int h, long long* seamCount = nullptr)
    {
        double best = std::numeric_limits<double>::infinity();
        long long count = 0;
        for (int start = 0; start < w; ++start)
            enumerateSeams(e, w, h, 0, start, 0.0, best, count);
        if (seamCount) *seamCount = count;
        return best;
    }

    bool isValidSeam(const Seam& seam, int w, int h)
    {
        if (static_cast<int>(seam.size()) != h) return false;
        for (int i = 0; i < h; ++i)
        {
            if (seam[i] < 0 || seam[i] >= w) return false;
            if (i > 0 && std::abs(seam[i] - seam[i - 1]) > 1) return false;
        }
        return true;
    }

    double seamCost(const EnergyMap& e, int w, const Seam& seam)
    {
        double sum = 0.0;
        for (size_t i = 0; i < seam.size(); ++i) sum += e[i * w + seam[i]];
        return sum;
    }

    Image randomImage(int w, int h, std::mt19937& rng)
    {
        std::uniform_int_distribution<int> d(0, 255);
        Image img(w, h);
        for (auto& p : img.pixels)
            p = { static_cast<uint8_t>(d(rng)), static_cast<uint8_t>(d(rng)), static_cast<uint8_t>(d(rng)) };
        return img;
    }

    // ---------------------------------------------------------------------

    void testHandWorkedExample()
    {
        std::cout << "Test 1: hand-worked 3x5 energy grid\n";
        //   1 4 3 5 2
        //   3 2 5 2 3
        //   5 2 4 2 2
        // Cumulative M:
        //   1 4 3 5 2
        //   4 3 8 4 5
        //   8 5 7 6 6     -> min 5 at column 1, seam (0,1,1)
        const int w = 5, h = 3;
        EnergyMap e = { 1,4,3,5,2,  3,2,5,2,3,  5,2,4,2,2 };
        double cost = 0.0;
        Seam seam = findVerticalSeamDP(e, w, h, &cost);
        check(seam == Seam({ 0, 1, 1 }), "seam is columns (0, 1, 1)");
        check(std::fabs(cost - 5.0) < 1e-12, "seam cost is 5");
    }

    void testAgainstBruteForce()
    {
        std::cout << "Test 2: DP vs brute force on 2000 random energy grids (1..7 x 1..8)\n";
        std::mt19937 rng(12345);
        std::uniform_int_distribution<int> dw(1, 7), dh(1, 8);
        std::uniform_real_distribution<double> de(0.0, 100.0);

        bool costsMatch = true, seamsValid = true, costConsistent = true;
        for (int t = 0; t < 2000; ++t)
        {
            const int w = dw(rng), h = dh(rng);
            EnergyMap e(static_cast<size_t>(w) * h);
            for (auto& v : e) v = std::round(de(rng)); // integers -> many ties, a harder case

            double dpCost = 0.0;
            Seam seam = findVerticalSeamDP(e, w, h, &dpCost);
            const double bfCost = bruteForceMinSeam(e, w, h);

            if (std::fabs(dpCost - bfCost) > 1e-9) costsMatch = false;
            if (!isValidSeam(seam, w, h)) seamsValid = false;
            if (std::fabs(seamCost(e, w, seam) - dpCost) > 1e-9) costConsistent = false;
        }
        check(costsMatch, "DP minimum equals brute-force minimum");
        check(seamsValid, "every seam is connected and in bounds");
        check(costConsistent, "returned seam actually sums to the reported cost");
    }

    void testEnergy()
    {
        std::cout << "Test 3: Sobel energy map\n";
        Image flat(10, 8);
        for (auto& p : flat.pixels) p = { 120, 60, 200 };
        EnergyMap e = computeEnergy(flat);
        check(std::all_of(e.begin(), e.end(), [](double v) { return std::fabs(v) < 1e-9; }),
              "uniform image has zero energy everywhere");

        // Left half black, right half white: energy only near the edge.
        Image edge(10, 6);
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 10; ++x)
                edge.at(x, y) = x < 5 ? Pixel{ 0, 0, 0 } : Pixel{ 255, 255, 255 };
        e = computeEnergy(edge);
        bool ok = true;
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 10; ++x)
            {
                const bool nearEdge = (x == 4 || x == 5);
                const double v = e[static_cast<size_t>(y) * 10 + x];
                if (nearEdge && v <= 0.0) ok = false;
                if (!nearEdge && std::fabs(v) > 1e-9) ok = false;
            }
        check(ok, "vertical edge produces energy only at the edge columns");

        // The minimum vertical seam on that image must avoid the edge.
        Seam seam = findVerticalSeamDP(e, 10, 6);
        check(std::none_of(seam.begin(), seam.end(), [](int x) { return x == 4 || x == 5; }),
              "minimum seam avoids the high-energy edge");
    }

    void testTranspose()
    {
        std::cout << "Test 4: transpose helpers\n";
        std::mt19937 rng(7);
        Image img = randomImage(13, 9, rng);
        Image back = transpose(transpose(img));
        bool same = back.width == img.width && back.height == img.height;
        for (size_t i = 0; same && i < img.pixels.size(); ++i)
            same = img.pixels[i].r == back.pixels[i].r && img.pixels[i].g == back.pixels[i].g
                && img.pixels[i].b == back.pixels[i].b;
        check(same, "transpose(transpose(img)) == img");

        std::vector<int> v(13 * 9);
        for (size_t i = 0; i < v.size(); ++i) v[i] = static_cast<int>(i);
        check(transpose(transpose(v, 13, 9), 9, 13) == v, "index transpose round-trips");
    }

    void testCarve()
    {
        std::cout << "Test 5: full carve pipeline\n";
        std::mt19937 rng(99);
        Image img = randomImage(40, 30, rng);

        CarveResult r = carve(img, 25, 20);
        check(r.output.width == 25 && r.output.height == 20, "40x30 -> 25x20 has the right size");
        check(r.stats.verticalSeamsRemoved == 15 && r.stats.horizontalSeamsRemoved == 10,
              "15 vertical and 10 horizontal seams removed");

        // Every removed pixel is drawn exactly once on the seam map.
        int marked = 0;
        for (size_t i = 0; i < img.pixels.size(); ++i)
        {
            const Pixel& m = r.seamMap.pixels[i];
            const Pixel& o = img.pixels[i];
            if (m.r != o.r || m.g != o.g || m.b != o.b) ++marked;
        }
        // (A random pixel could coincidentally already be pure red/blue, so allow <=.)
        check(marked <= 40 * 30 - 25 * 20 && marked >= 40 * 30 - 25 * 20 - 2,
              "seam map marks the 700 removed pixels");

        CarveResult tiny = carve(img, 1, 1);
        check(tiny.output.width == 1 && tiny.output.height == 1, "can shrink all the way to 1x1");

        CarveResult same = carve(img, 40, 30);
        check(same.output.width == 40 && same.output.height == 30, "target == original is a no-op");

        bool threw = false;
        try { carve(img, 41, 30); } catch (const std::invalid_argument&) { threw = true; }
        check(threw, "enlarging is rejected (Q1 only requires reduction)");
    }
}

void printSeamCountTable()
{
    std::cout << "\nNumber of valid seams in an m x n image (brute-force count)\n";
    std::cout << "   m |    n=2 |    n=3 |    n=4 |      2^m\n";
    std::cout << "  ---+--------+--------+--------+---------\n";
    for (int m = 1; m <= 12; ++m)
    {
        long long counts[3];
        for (int n = 2; n <= 4; ++n)
        {
            EnergyMap e(static_cast<size_t>(m) * n, 0.0);
            bruteForceMinSeam(e, n, m, &counts[n - 2]);
        }
        std::printf("  %2d | %6lld | %6lld | %6lld | %7lld\n",
                    m, counts[0], counts[1], counts[2], 1LL << m);
    }
    std::cout << "  n=2 gives exactly 2^m and larger n only adds seams, so the count is\n"
              << "  at least 2^m for any n > 1: exponential in m.\n";
}

bool runAllTests()
{
    g_passed = g_failed = 0;
    std::cout << "\n=== Running correctness tests ===\n";
    testHandWorkedExample();
    testAgainstBruteForce();
    testEnergy();
    testTranspose();
    testCarve();
    std::cout << "\nResult: " << g_passed << " passed, " << g_failed << " failed.\n";
    printSeamCountTable();
    return g_failed == 0;
}
