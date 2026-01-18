#!/bin/bash
mkdir dill
cd dill
git clone https://github.com/GTKorvo/dill.git source
mkdir build
cd build
cmake \
  -DCMAKE_BUILD_TYPE=$1 \
  -DBUILD_TESTING=OFF \
  -DCMAKE_INSTALL_PREFIX=${PWD}/../install \
  ../source
cmake --build . -j4 --config $1
cmake --install . --config $1

# Add DLL directory to PATH for runtime discovery (Windows only)
if [ -d "${PWD}/../install/bin" ] && [ -n "${GITHUB_PATH:-}" ]; then
    # Convert to Windows-style path and add to GitHub Actions PATH
    DILL_BIN_DIR=$(cd "${PWD}/../install/bin" && pwd)
    echo "${DILL_BIN_DIR}" >> "${GITHUB_PATH}"
fi

