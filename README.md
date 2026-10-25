# Seam Carving (Dynamic Programming and Greedy)

Content-aware image shrinking with vertical **and** horizontal seams, written in C++17. There are no external dependencies: image reading and writing use the single-header `stb_image` libraries, which are included in the project. All of the seam-carving logic is hand-written.

## Run in Visual Studio (2022 or newer)

1. Open `SeamCarving.sln`.
2. Set the configuration to **Release | x64** for speed. Debug works but is about 10–20× slower.
3. Press **F5** or **Ctrl+F5**. A console menu appears:
   - `1` resizes an image with either `dp` or `greedy`.
   - `2` runs the correctness tests and prints the seam-count table used for Q2 a(i).
   - `3` runs both algorithms and writes a comparison sheet.
   - `4` exits.

The menu comes back after every action, so you can run several resizes or comparisons in one session. Choose `4` (or close the window) when you are done.

Visual Studio runs the program from the `SeamCarving/` project folder. Put your own images in `SeamCarving/input/`, next to `sample.png`, or type a full path. Every generated image is written to `SeamCarving/output/`.

If Visual Studio offers to **retarget** the project (a newer VS with a different toolset), click OK.

## One-click build / clean

If you do not want to type commands, use the batch files in [`bat/`](file:///c:/Users/User/source/repos/ass2/ass2/bat):

| File | What it does |
|---|---|
| [`bat/build.bat`](file:///c:/Users/User/source/repos/ass2/ass2/bat/build.bat) | configures CMake into `build/` and builds the **Release** executable |
| [`bat/clean.bat`](file:///c:/Users/User/source/repos/ass2/ass2/bat/clean.bat) | removes `.vs`, `x64`, `build`, `SeamCarving/x64`, and clears the generated images in `SeamCarving/output/` |

How to use them:

1. Open the repo folder.
2. Open the `bat` folder.
3. Double-click `build.bat` to build, or `clean.bat` to remove local build folders.
4. Read the message in the window, then press any key to close it.

## CMake

The repo uses `CMakeLists.txt` as the build configuration, with one extra local clean target:

| File | Target | What it does |
|---|---|---|
| `CMakeLists.txt` | `clean-local` | removes `.vs`, `x64`, and `SeamCarving/x64`, and clears the generated images in `SeamCarving/output/` |

Examples:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release    # configure
cmake --build build --config Release              # build
cmake --build build --config Release --target clean-local   # clean source tree + output images
```

## Program command line

```
SeamCarving.exe <input> <width> <height> [dp|greedy]
SeamCarving.exe --compare <input> <width> <height>
SeamCarving.exe --test
```

There is no output path argument: the input name and the algorithm decide the file names, and everything is written to `output/`.

Example, shrinking `input/astronaut.png` to 384x512 and comparing both algorithms:

```text
SeamCarving.exe --compare input/astronaut.png 384 512
```

## Project layout

| Folder | Contents |
|---|---|
| `SeamCarving/input/` | the source images you feed to the program (`sample.png`, `astronaut.png`, `coffee.png`) |
| `SeamCarving/output/` | every image the program writes |

How to use them:

1. Copy or save the images you want to shrink into `SeamCarving/input/`.
2. Run the program (press **F5**, or `build.bat`, or the command line).
3. Type the input file name, for example `input/astronaut.png`. Pressing Enter accepts `input/sample.png`.
4. Answer the size and algorithm prompts - there is no output prompt, the folder is always `output/`.
5. Open `SeamCarving/output/` to see the results. Nothing is written back into `input/`.
6. To empty `output/` again, run `bat/clean.bat` or the CMake `clean-local` target. Only the `.gitkeep` placeholder is kept.

## Outputs

The file name is `<original name>_<algorithm>`, so `input/astronaut.png` shrunk with `dp` produces:

| File | Contents |
|---|---|
| `output/output_astronaut_dp.png` | the resized image |
| `output/output_energy_astronaut_dp.png` | Sobel energy map of the original image |
| `output/output_seams_astronaut_dp.png` | the original image with every removed seam drawn (red = vertical, blue = horizontal) |

Running the same image with `greedy` writes the matching `..._greedy` set, so the two results sit side by side and never overwrite each other.

The console also prints timing split into energy computation, seam search and seam removal, plus the total removed energy.

`--compare input/astronaut.png` writes both sets plus one extra sheet:

| File | Contents |
|---|---|
| `output/output_astronaut_dp.png` | DP resized result |
| `output/output_astronaut_greedy.png` | greedy resized result |
| `output/output_seams_astronaut_dp.png` | original image with DP seams |
| `output/output_seams_astronaut_greedy.png` | original image with greedy seams |
| `output/output_energy_astronaut_dp.png` | Sobel energy map used by DP |
| `output/output_energy_astronaut_greedy.png` | Sobel energy map used by greedy (identical to the DP one today) |
| `output/output_astronaut_comparison.png` | 2x3 sheet: DP seams, greedy seams, seam difference, original, DP result, greedy result |

## Code layout

| File | Purpose |
|---|---|
| `SeamCarver.h/.cpp` | (a) Sobel energy, (b) DP minimum seam, (b alt) greedy seam, (c) seam removal, transpose for horizontal seams, full `carve()` loop |
| `Tests.h/.cpp` | DP checked against brute-force enumeration of every seam, plus energy, transpose, pipeline and greedy-vs-DP tests |
| `ImageIO.cpp`, `Image.h` | image container and stb load/save |
| `main.cpp` | interactive menu, algorithm selection and comparison outputs |
| `CMakeLists.txt`, `bat/*.bat` | build / clean helpers for command-line and double-click workflows |

## Algorithm summary

- **Energy:** `e(x,y) = sqrt(Gx² + Gy²)` on grayscale, using 3×3 Sobel kernels with clamped borders.
- **DP:** `M[i][j] = e[i][j] + min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])`. Take the smallest value in the last row and backtrack through the stored parent offsets.
- **Greedy:** start from the minimum-energy pixel in row 0, then on each next row choose the cheapest reachable pixel among `{col-1, col, col+1}`.
- **Horizontal seams:** transpose the image, remove vertical seams, transpose back.
- **Cost per seam:** energy and removal are O(W·H) either way. DP seam search is O(W·H); greedy seam search is O(W+H).

`SeamCarver.h` defines a `SeamFinder` type, so greedy and later seam finders can be passed into `carve()` without changing the rest of the pipeline.
