#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status.

# Navigate to the cpp_backend directory
cd /home/nirantar/Downloads/seperate_1/youtube-topic-dataset/backend/ALGOBAAP/cpp_backend

echo "Cleaning previous build artifacts..."
# Remove the build directory entirely for a clean start
rm -rf build

echo "Creating build directory..."
mkdir -p build

echo "Running CMake configuration from build directory..."
# Run CMake to configure the project, specifying the source directory from the build directory's perspective
cmake -S . -B build

echo "Verifying Makefile generation..."
# Check if CMake successfully generated Makefiles in the build directory
if [ ! -f build/Makefile ]; then
    echo "Error: CMake failed to generate Makefile in the 'build' directory. Please check CMake output above for errors."
    exit 1
fi

echo "Building the project from build directory..."
# Build the project, explicitly specifying the build directory and the target executable
cmake --build build --target youtube-topic-crow

echo "Verifying executable creation..."
# Check if the executable was created
if [ ! -f build/youtube-topic-crow ]; then
    echo "Error: Executable 'youtube-topic-crow' not found after build. Please check build output above for errors."
    exit 1
fi

echo "C++ backend build complete. Executable should be in ./build/youtube-topic-crow"