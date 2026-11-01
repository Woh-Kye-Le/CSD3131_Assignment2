// SeamCarver.cpp
#include "SeamCarver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <queue>

namespace sc
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        double msSince(Clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        }

        // Dinic's algorithm: BFS builds a level graph; DFS sends blocking flow.
        // Each directed edge has its own initially-zero reverse residual edge.
        // These residual edges are separate from the graph's INF constraint edges.
        class FlowGraph
        {
            using Capacity = long double;
            struct Edge { int to; int reverse; Capacity residual; };
            std::vector<std::vector<Edge>> edges;
            std::vector<int> level;
            std::vector<size_t> nextEdge;

            bool buildLevels(int source, int sink)
            {
                std::fill(level.begin(), level.end(), -1);
                std::queue<int> q;
                level[source] = 0;
                q.push(source);
                while (!q.empty())
                {
                    const int u = q.front(); q.pop();
                    for (const Edge& e : edges[u])
                        if (e.residual > 0 && level[e.to] < 0)
                        {
                            level[e.to] = level[u] + 1;
                            q.push(e.to);
                        }
                }
                return level[sink] >= 0;
            }

            Capacity send(int u, int sink, Capacity limit)
            {
                if (u == sink) return limit;
                for (size_t& i = nextEdge[u]; i < edges[u].size(); ++i)
                {
                    Edge& e = edges[u][i];
                    if (e.residual <= 0 || level[e.to] != level[u] + 1) continue;
                    const Capacity pushed = send(e.to, sink, std::min(limit, e.residual));
                    if (pushed > 0)
                    {
                        e.residual -= pushed;
                        edges[e.to][e.reverse].residual += pushed;
                        return pushed;
                    }
                }
                return 0;
            }

        public:
            explicit FlowGraph(int count) : edges(count), level(count), nextEdge(count) {}

            void addEdge(int from, int to, Capacity capacity)
            {
                const int reverse = static_cast<int>(edges[to].size());
                const int forward = static_cast<int>(edges[from].size());
                edges[from].push_back({ to, reverse, capacity });
                edges[to].push_back({ from, forward, 0 });
            }

            void maxFlow(int source, int sink, Capacity limit)
            {
                while (buildLevels(source, sink))
                {
                    std::fill(nextEdge.begin(), nextEdge.end(), 0);
                    while (send(source, sink, limit) > 0) {}
                }
            }

            std::vector<bool> sourceSide(int source) const
            {
                std::vector<bool> reached(edges.size(), false);
                std::queue<int> q;
                reached[source] = true;
                q.push(source);
                while (!q.empty())
                {
                    const int u = q.front(); q.pop();
                    for (const Edge& e : edges[u])
                        if (e.residual > 0 && !reached[e.to])
                        {
                            reached[e.to] = true;
                            q.push(e.to);
                        }
                }
                return reached;
            }
        };

        // ITU-R BT.601 luma.
        inline double luminance(const Pixel& p)
        {
            return 0.299 * p.r + 0.587 * p.g + 0.114 * p.b;
        }

        // Greedy tie-breaking: straight first, then left, then right.
        int bestNextColumn(const double* row, int width, int col)
        {
            int bestCol = col;
            double best = row[col];
            if (col > 0 && row[col - 1] < best)
            {
                best = row[col - 1];
                bestCol = col - 1;
            }
            if (col < width - 1 && row[col + 1] < best)
                bestCol = col + 1;
            return bestCol;
        }
    }

    // ------------------------------------------------------------------------
    // (a) Energy map: Sobel gradient magnitude
    //
    //       Gx = [-1 0 +1]      Gy = [-1 -2 -1]
    //            [-2 0 +2]           [ 0  0  0]
    //            [-1 0 +1]           [+1 +2 +1]
    //
    // Time O(W*H), space O(W*H).
    // ------------------------------------------------------------------------
    EnergyMap computeEnergy(const Image& img)
    {
        const int w = img.width;
        const int h = img.height;

        // Grayscale once so each pixel is converted only one time.
        std::vector<double> gray(static_cast<size_t>(w) * h);
        for (size_t i = 0; i < gray.size(); ++i)
            gray[i] = luminance(img.pixels[i]);

        // Clamped lookup replicates edge pixels at the borders.
        auto G = [&](int x, int y) -> double {
            x = std::clamp(x, 0, w - 1);
            y = std::clamp(y, 0, h - 1);
            return gray[static_cast<size_t>(y) * w + x];
        };

        EnergyMap energy(static_cast<size_t>(w) * h);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const double gx =
                    -G(x - 1, y - 1) + G(x + 1, y - 1)
                    - 2.0 * G(x - 1, y) + 2.0 * G(x + 1, y)
                    - G(x - 1, y + 1) + G(x + 1, y + 1);

                const double gy =
                    -G(x - 1, y - 1) - 2.0 * G(x, y - 1) - G(x + 1, y - 1)
                    + G(x - 1, y + 1) + 2.0 * G(x, y + 1) + G(x + 1, y + 1);

                energy[static_cast<size_t>(y) * w + x] = std::sqrt(gx * gx + gy * gy);
            }
        }
        return energy;
    }

    // ------------------------------------------------------------------------
    // (b) Minimum vertical seam by dynamic programming
    //
    // Subproblem: M[i][j] = minimum total energy of any connected seam that
    // starts in row 0 and ends at pixel (i, j).
    //
    //   M[0][j] = e[0][j]
    //   M[i][j] = e[i][j] + min( M[i-1][j-1], M[i-1][j], M[i-1][j+1] )
    //             (neighbours outside [0, W-1] are skipped)
    //
    // Answer: min_j M[H-1][j]; the seam is recovered by following the stored
    // parent offsets (-1, 0, +1) back up to row 0.
    //
    // Time O(W*H): each cell looks at 3 cells above it.
    // Space O(W*H) for M and the parent table (see report for O(W) variants).
    // ------------------------------------------------------------------------
    Seam findVerticalSeamDP(const EnergyMap& energy, int width, int height, double* totalCost)
    {
        const int w = width;
        const int h = height;

        std::vector<double> M(energy.begin(), energy.end()); // row 0 is already correct
        std::vector<int8_t> parent(static_cast<size_t>(w) * h, 0);   // offset to parent column

        for (int i = 1; i < h; ++i)
        {
            const double* prev = &M[static_cast<size_t>(i - 1) * w];
            double* cur = &M[static_cast<size_t>(i) * w];
            int8_t* par = &parent[static_cast<size_t>(i) * w];

            for (int j = 0; j < w; ++j)
            {
                // Prefer straight up on ties, then left, then right
                // (deterministic, and keeps seams as straight as possible).
                double best = prev[j];
                int8_t offset = 0;
                if (j > 0 && prev[j - 1] < best)     { best = prev[j - 1]; offset = -1; }
                if (j < w - 1 && prev[j + 1] < best) { best = prev[j + 1]; offset = +1; }

                cur[j] += best;      // cur[j] held e[i][j]; now holds M[i][j]
                par[j] = offset;
            }
        }

        // Smallest cumulative energy in the bottom row.
        const double* last = &M[static_cast<size_t>(h - 1) * w];
        int col = static_cast<int>(std::min_element(last, last + w) - last);
        if (totalCost) *totalCost = last[col];

        // Backtrack.
        Seam seam(h);
        for (int i = h - 1; i >= 0; --i)
        {
            seam[i] = col;
            col += parent[static_cast<size_t>(i) * w + col];
        }
        return seam;
    }

    // ------------------------------------------------------------------------
    // (b alt) Greedy vertical seam
    //
    // Pick the smallest-energy pixel in row 0, then keep extending the seam
    // by choosing the cheapest reachable pixel on the next row. This is
    // O(W + H) time and O(H) space, but it is not globally optimal.
    // ------------------------------------------------------------------------
    Seam findVerticalSeamGreedy(const EnergyMap& energy, int width, int height, double* totalCost)
    {
        const int w = width;
        const int h = height;

        Seam seam(h);
        const double* row0 = energy.data();
        int col = static_cast<int>(std::min_element(row0, row0 + w) - row0);

        double cost = 0.0;
        seam[0] = col;
        cost += row0[col];

        for (int y = 1; y < h; ++y)
        {
            const double* row = &energy[static_cast<size_t>(y) * w];
            col = bestNextColumn(row, w, col);
            seam[y] = col;
            cost += row[col];
        }

        if (totalCost) *totalCost = cost;
        return seam;
    }

    // ------------------------------------------------------------------------
    // (c) Remove a vertical seam: shift every pixel right of the seam one
    // place left in each row. Time O(W*H).
    // ------------------------------------------------------------------------
    void removeVerticalSeam(Image& img, std::vector<int>& origin, const Seam& seam)
    {
        const int w = img.width;
        const int h = img.height;
        const int newW = w - 1;

        Image out(newW, h);
        std::vector<int> newOrigin(static_cast<size_t>(newW) * h);

        for (int y = 0; y < h; ++y)
        {
            const int cut = seam[y];
            const size_t src = static_cast<size_t>(y) * w;
            const size_t dst = static_cast<size_t>(y) * newW;

            // [0, cut) stays, [cut+1, w) shifts left by one.
            std::copy(img.pixels.begin() + src, img.pixels.begin() + src + cut, out.pixels.begin() + dst);
            std::copy(img.pixels.begin() + src + cut + 1, img.pixels.begin() + src + w, out.pixels.begin() + dst + cut);

            std::copy(origin.begin() + src, origin.begin() + src + cut, newOrigin.begin() + dst);
            std::copy(origin.begin() + src + cut + 1, origin.begin() + src + w, newOrigin.begin() + dst + cut);
        }

        img = std::move(out);
        origin = std::move(newOrigin);
    }

    // ------------------------------------------------------------------------
    // Q4: minimum s-t cut. Node (x,y), 0 <= x < W, denotes a pixel;
    // node (W,y) is an extra right boundary fixed to the sink side.
    //
    // S -> (0,y) : INF                 first pixel must be on source side
    // (W,y) -> T : INF                 boundary must be on sink side
    // (x,y) -> (x+1,y) : energy[y*W+x] pays for selected pixel x
    // (x+1,y) -> (x,y) : INF           source-side nodes form a row prefix
    // (x,y) -> (x-1,y +/- 1) : INF     adjacent row prefixes differ by <= 1
    //
    // Thus the rightmost source-side pixel in each row is the seam.
    // Every finite cut pays for exactly one pixel per row, and every valid
    // seam defines such a cut. Min-cut therefore minimizes the same cost as DP.
    // Build/storage O(W*H); general Dinic bound O(V^2 E) = O((W*H)^3).
    // ------------------------------------------------------------------------
    Seam findVerticalSeamGraphCut(const EnergyMap& energy, int width, int height, double* totalCost)
    {
        if (width < 1 || height < 1 ||
            static_cast<size_t>(width) * height != energy.size())
            throw std::invalid_argument("Graph cut requires a non-empty, correctly sized energy map.");
        const long long nodeCount = (static_cast<long long>(width) + 1) * height + 2;
        if (nodeCount > std::numeric_limits<int>::max())
            throw std::length_error("Graph cut graph exceeds the supported node count.");

        long double sum = 0;
        for (double value : energy)
        {
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("Graph cut energies must be finite and non-negative.");
            sum += value;
        }
        // Larger than the cost of ANY seam. Finite avoids infinity-infinity
        // during residual updates. long double reduces rounding accumulation.
        const long double inf = sum + std::max(1.0L, sum);
        if (!std::isfinite(inf))
            throw std::overflow_error("Graph cut capacities exceed the supported range.");

        const int stride = width + 1;
        const int source = static_cast<int>(nodeCount) - 2;
        const int sink = source + 1;
        FlowGraph graph(sink + 1);
        auto node = [stride](int x, int y) { return y * stride + x; };
        for (int y = 0; y < height; ++y)
        {
            graph.addEdge(source, node(0, y), inf);
            graph.addEdge(node(width, y), sink, inf);
            for (int x = 0; x < width; ++x)
            {
                graph.addEdge(node(x, y), node(x + 1, y), energy[static_cast<size_t>(y) * width + x]);
                graph.addEdge(node(x + 1, y), node(x, y), inf);
                if (x > 0)
                {
                    if (y > 0) graph.addEdge(node(x, y), node(x - 1, y - 1), inf);
                    if (y + 1 < height) graph.addEdge(node(x, y), node(x - 1, y + 1), inf);
                }
            }
        }
        graph.maxFlow(source, sink, inf);
        const auto reached = graph.sourceSide(source);
        Seam seam(height);
        double cost = 0;
        for (int y = 0; y < height; ++y)
        {
            int x = 0;
            while (x + 1 < width && reached[node(x + 1, y)]) ++x;
            seam[y] = x;
            cost += energy[static_cast<size_t>(y) * width + x];
        }
        if (totalCost) *totalCost = cost;
        return seam;
    }

    // ------------------------------------------------------------------------
    // Transpose helpers (used to turn horizontal seams into vertical ones).
    // ------------------------------------------------------------------------
    Image transpose(const Image& img)
    {
        Image t(img.height, img.width);
        for (int y = 0; y < img.height; ++y)
            for (int x = 0; x < img.width; ++x)
                t.at(y, x) = img.at(x, y);
        return t;
    }

    std::vector<int> transpose(const std::vector<int>& data, int width, int height)
    {
        std::vector<int> t(data.size());
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                t[static_cast<size_t>(x) * height + y] = data[static_cast<size_t>(y) * width + x];
        return t;
    }

    Image energyToImage(const EnergyMap& energy, int width, int height)
    {
        Image out(width, height);
        const double maxE = energy.empty() ? 0.0 : *std::max_element(energy.begin(), energy.end());
        const double scale = maxE > 0.0 ? 255.0 / maxE : 0.0;
        for (size_t i = 0; i < energy.size(); ++i)
        {
            const auto v = static_cast<uint8_t>(std::lround(energy[i] * scale));
            out.pixels[i] = { v, v, v };
        }
        return out;
    }

    // ------------------------------------------------------------------------
    // Full pipeline: repeat (a) energy -> (b) find seam -> (c) remove.
    // ------------------------------------------------------------------------
    CarveResult carve(const Image& input, int targetWidth, int targetHeight, const SeamFinder& finder)
    {
        if (targetWidth < 1 || targetHeight < 1 || targetWidth > input.width || targetHeight > input.height)
            throw std::invalid_argument("Target size must be between 1x1 and the original size.");

        const auto start = Clock::now();
        CarveResult result;
        CarveStats& stats = result.stats;

        Image img = input;

        // origin[k] = index of this pixel in the ORIGINAL image.
        std::vector<int> origin(input.pixels.size());
        for (size_t i = 0; i < origin.size(); ++i) origin[i] = static_cast<int>(i);

        // 0 = kept, 1 = removed by a vertical seam, 2 = removed by a horizontal seam.
        std::vector<uint8_t> removedBy(input.pixels.size(), 0);

        // Removes `count` vertical seams from img (whatever orientation it is in).
        auto removeSeams = [&](int count, uint8_t tag) {
            for (int s = 0; s < count; ++s)
            {
                auto t0 = Clock::now();
                const EnergyMap energy = computeEnergy(img);       // (a) recompute every iteration
                stats.energyMs += msSince(t0);

                t0 = Clock::now();
                double seamCost = 0.0;
                const Seam seam = finder(energy, img.width, img.height, &seamCost); // (b)
                stats.seamMs += msSince(t0);
                stats.removedEnergy += seamCost;

                for (int y = 0; y < img.height; ++y)
                    removedBy[origin[static_cast<size_t>(y) * img.width + seam[y]]] = tag;

                t0 = Clock::now();
                removeVerticalSeam(img, origin, seam);             // (c)
                stats.removeMs += msSince(t0);
            }
        };

        // Vertical seams: reduce width.
        stats.verticalSeamsRemoved = input.width - targetWidth;
        removeSeams(stats.verticalSeamsRemoved, 1);

        // Horizontal seams: transpose, remove vertical seams, transpose back.
        stats.horizontalSeamsRemoved = input.height - targetHeight;
        if (stats.horizontalSeamsRemoved > 0)
        {
            origin = transpose(origin, img.width, img.height);
            img = transpose(img);
            removeSeams(stats.horizontalSeamsRemoved, 2);
            origin = transpose(origin, img.width, img.height);
            img = transpose(img);
        }

        // Seam visualisation on the original image.
        result.seamMap = input;
        for (size_t i = 0; i < removedBy.size(); ++i)
        {
            if (removedBy[i] == 1) result.seamMap.pixels[i] = { 255, 0, 0 };
            else if (removedBy[i] == 2) result.seamMap.pixels[i] = { 0, 80, 255 };
        }

        result.output = std::move(img);
        result.removedBy = std::move(removedBy);
        stats.totalMs = msSince(start);
        return result;
    }
}
