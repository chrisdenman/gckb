#!/bin/bash

if [ ! -d ".github" ]; then
  echo "'./.github' directory does not exist, are you in the home directory?" >&2
  exit 1
fi

readonly BUILD_TREE="build"
readonly SOURCE_TREE="."
readonly GENERATOR="Ninja"

# Generate a Project Buildsystem : cmake [<options>] -B <path-to-build> [-S <path-to-source>]
cmake -Wdev --fresh -B "${BUILD_TREE}" -S "${SOURCE_TREE}" -G "${GENERATOR}" -DCMAKE_BUILD_TYPE="Release"

# Build a Project : cmake −−build <dir> [<options>] [−− <build−tool−options>]
cmake --build "${BUILD_TREE}" --clean-first  --parallel 12 --verbose

# Install the project with ninja
sudo ninja -C "${BUILD_TREE}" install