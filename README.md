# Seam Carving – Question 1 (Dynamic Programming)

Content-aware image shrinking with vertical **and** horizontal seams, written in C++17. There are no external dependencies: image reading and writing use the single-header `stb_image` libraries, which are included in the project. All of the seam-carving logic is hand-written.

## Run in Visual Studio (2022 or newer)

1. Open `SeamCarving.sln`.
2. Set the configuration to **Release | x64** for speed. Debug works but is about 10–20× slower.
3. Press **F5** or **Ctrl+F5**. A console menu appears:
   - `1` resizes an image. Pressing Enter accepts `sample.png`, then you type a target width and height.
   - `2` runs the correctness tests and prints the seam-count table used for Q2 a(i).

Visual Studio runs the program from the `SeamCarving/` project folder. Put your own images there, next to `sample.png`, or type a full path. The output files are written to the same folder.

If Visual Studio offers to **retarget** the project (a newer VS with a different toolset), click OK.

## Command line

```
SeamCarving.exe <input> <output> <targetWidth> <targetHeight>
SeamCarving.exe --test
```

## Outputs

For an output named `output.png`:

| File | Contents |
|---|---|
| `output.png` | the resized image |
| `output_energy.png` | Sobel energy map of the original image |
| `output_seams.png` | the original image with every removed seam drawn (red = vertical, blue = horizontal) |

The console also prints timing split into energy computation, DP seam search and seam removal.

## Code layout

| File | Purpose |
|---|---|
| `SeamCarver.h/.cpp` | (a) Sobel energy, (b) DP minimum seam, (c) seam removal, transpose for horizontal seams, full `carve()` loop |
| `Tests.h/.cpp` | DP checked against brute-force enumeration of every seam, plus energy, transpose and pipeline tests |
| `ImageIO.cpp`, `Image.h` | image container and stb load/save |
| `main.cpp` | interactive menu and command-line interface |

## Algorithm summary

- **Energy:** `e(x,y) = sqrt(Gx² + Gy²)` on grayscale, using 3×3 Sobel kernels with clamped borders.
- **DP:** `M[i][j] = e[i][j] + min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])`. Take the smallest value in the last row and backtrack through the stored parent offsets.
- **Horizontal seams:** transpose the image, remove vertical seams, transpose back.
- **Cost per seam:** O(W·H) time for the energy map, the DP and the removal. Removing k seams costs O(k·W·H).

`SeamCarver.h` defines a `SeamFinder` type, so the greedy (Q3) and graph-cut (Q4) seam finders can be passed into `carve()` without changing the rest of the pipeline.
