#!/bin/bash
mkdir atl
cd atl
git clone https://github.com/GTKorvo/atl.git source
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
    # Convert to Windows-native path format for DLL loading
    ATL_BIN_DIR=$(cd "${PWD}/../install/bin" && pwd -W 2>/dev/null || cygpath -w "$(pwd)")
    echo "${ATL_BIN_DIR}" >> "${GITHUB_PATH}"
fi
