#!/bin/bash

readonly BUILD_TREE="build"
readonly SOURCE_TREE="."
readonly GENERATOR="Ninja"

if [ ! -d ".git" ]; then
  echo "'./.git' directory does not exist, are you in the home directory?" >&2
  exit 1
fi

# Check the required dependencies are installed
for cmd in "cmake" "pkgconf"; do
  &>/dev/null which "${cmd}" \
    || >&2 echo "${cmd} is not installed, please install it using \"sudo apt-get install ${cmd}\""
done;
dpkg-query --show --showformat='${binary:Package}\n' | grep --quiet --regexp "libglib2\..-dev.*" \
  || >&2 echo "libglib2.0-dev is not installed, please install with \"sudo apt-get install libglib2.0-dev\""

# Generate a Project Buildsystem : cmake [<options>] -B <path-to-build> [-S <path-to-source>]
cmake -Wdev --fresh -B "${BUILD_TREE}" -S "${SOURCE_TREE}" -G "${GENERATOR}" -DCMAKE_BUILD_TYPE="Release"

# Build a Project : cmake −−build <dir> [<options>] [−− <build−tool−options>]
cmake --build "${BUILD_TREE}" --clean-first  --parallel 12 --verbose

# Install the project with ninja
sudo ninja -C "${BUILD_TREE}" install