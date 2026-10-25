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

# cleans build and unwanted files from the source tree, and clears the
# generated images in SeamCarving/output.
add_custom_target(clean-local
  COMMAND ${CMAKE_COMMAND} -E rm -rf
          "${CMAKE_SOURCE_DIR}/.vs"
          "${CMAKE_SOURCE_DIR}/x64"
          "${CMAKE_SOURCE_DIR}/SeamCarving/x64"
  COMMAND ${CMAKE_COMMAND} -E rm -f
          "${CMAKE_SOURCE_DIR}/SeamCarving/SeamCarving.vcxproj.user"
  COMMAND ${CMAKE_COMMAND} -E rm -rf
          "${CMAKE_SOURCE_DIR}/SeamCarving/output"
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${CMAKE_SOURCE_DIR}/SeamCarving/output"
  COMMAND ${CMAKE_COMMAND} -E touch
          "${CMAKE_SOURCE_DIR}/SeamCarving/output/.gitkeep"
  COMMAND ${CMAKE_COMMAND} -E echo
          "Removed .vs, x64, SeamCarving/x64, SeamCarving.vcxproj.user, and cleared SeamCarving/output.")
