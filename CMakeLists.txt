# Choose a reasonably modern but compatible version of CMake.
#
# At time of writing, CMake 3.5.1 was the version in the oldest supported
# Ubuntu LTS release (Xenial). Fedora, CentOS (with cmake3), Homebrew,
# and Chocolatey all provided at least 3.5.1 as well.
cmake_minimum_required(VERSION 3.5.1)

project(safeside VERSION 0.1.0 LANGUAGES C CXX)

# Use C++11 without extensions
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS off)

add_subdirectory(benchmarks)
add_subdirectory(demos)
add_subdirectory(kernel_modules)
add_subdirectory(experimental)