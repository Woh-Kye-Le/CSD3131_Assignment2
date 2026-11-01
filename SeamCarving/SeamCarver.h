// SeamCarver.h
// Seam carving (Avidan & Shamir, 2007) using dynamic programming.
//
// Horizontal seams are handled by transposing the image, removing vertical
// seams, and transposing back, so only one seam finder is needed.
#pragma once

#include "Image.h"

#include <functional>
#include <vector>

namespace sc
{
    // Energy value per pixel, row-major (index = y * width + x).
    using EnergyMap = std::vector<double>;

    // A vertical seam: seam[y] = column of the pixel removed in row y.
    using Seam = std::vector<int>;

    // Signature shared by every seam-finding strategy, so later questions
    // (greedy, graph cut) can be plugged into carve() without other changes.
    // If totalCost is non-null it receives the seam's summed energy.
    using SeamFinder = std::function<Seam(const EnergyMap&, int width, int height, double* totalCost)>;

    // ---- Step (a): energy map --------------------------------------------
    // Sobel gradient magnitude of the grayscale image: sqrt(Gx^2 + Gy^2).
    // Borders use clamped (replicated) neighbours.
    EnergyMap computeEnergy(const Image& img);

    // ---- Step (b): minimum seam by dynamic programming -------------------
    // M[i][j] = e[i][j] + min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])
    // Backtracks from the smallest entry in the last row.
    Seam findVerticalSeamDP(const EnergyMap& energy, int width, int height, double* totalCost = nullptr);

    // ---- Step (b alt): vertical seam by greedy row-by-row search --------
    // Starts at the smallest pixel in row 0, then on each later row picks
    // the cheapest reachable pixel among {col-1, col, col+1}.
    Seam findVerticalSeamGreedy(const EnergyMap& energy, int width, int height, double* totalCost = nullptr);

    // ---- Step (c): seam removal ------------------------------------------
    // Removes one pixel per row. `origin` (same layout as img.pixels) tracks
    // each pixel's index in the ORIGINAL image so seams can be drawn later.
    void removeVerticalSeam(Image& img, std::vector<int>& origin, const Seam& seam);

    // Find a minimum-energy vertical seam using minimum cut / maximum flow.
    Seam findVerticalSeamGraphCut(
        const EnergyMap& energy,
        int width,
        int height,
        double* totalCost = nullptr
    );

    // Helpers
    Image transpose(const Image& img);
    std::vector<int> transpose(const std::vector<int>& data, int width, int height);
    Image energyToImage(const EnergyMap& energy, int width, int height); // normalised grayscale

    struct CarveStats
    {
        int verticalSeamsRemoved = 0;
        int horizontalSeamsRemoved = 0;
        double removedEnergy = 0.0; // sum of the energies of the removed seams
        double energyMs = 0.0;   // time spent recomputing energy maps
        double seamMs = 0.0;     // time spent finding seams
        double removeMs = 0.0;   // time spent removing seams
        double totalMs = 0.0;
    };

    struct CarveResult
    {
        Image output;   // resized image
        Image seamMap;  // original image with removed seams drawn (red = vertical, blue = horizontal)
        std::vector<uint8_t> removedBy; // 0 kept, 1 removed by a vertical seam, 2 by a horizontal seam
        CarveStats stats;
    };

    // Shrinks `input` to targetWidth x targetHeight. Vertical seams are removed
    // first, then horizontal seams. Requires 1 <= target <= original size.
    CarveResult carve(const Image& input, int targetWidth, int targetHeight,
                      const SeamFinder& finder = findVerticalSeamDP);
}
