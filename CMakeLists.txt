# Optional: build without the .sln (macOS/Linux, or VS "Open Folder").
cmake_minimum_required(VERSION 3.15)
project(SeamCarving CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()
add_executable(SeamCarving
  SeamCarving/main.cpp
  SeamCarving/SeamCarver.cpp
  SeamCarving/ImageIO.cpp
  SeamCarving/Tests.cpp)
