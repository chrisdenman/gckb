#!/bin/bash

if [ ! -d ".github" ]; then
  echo "'./.github' directory does not exist, are you in the home directory?" >&2
  exit 1
fi

BUILD_TYPE="Release"
BUILD_DIR="build/cmake-build-${BUILD_TYPE,,}"
mkdir -p "${BUILD_DIR}"
cmake -S . -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" --target all --clean-first  -j 12