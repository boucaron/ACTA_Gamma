# Help me


## Start Devevel

### MinGW64
pacman -Syu
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-qt6 mingw-w64-x86_64-cmake

export MSYS2_ARG_CONV_EXCL="*"
export CC=/mingw64/bin/x86_64-w64-mingw32-gcc
export CXX=/mingw64/bin/x86_64-w64-mingw32-g++
cmake -G "Unix Makefiles" \
  -S . -B build \
  -DCMAKE_PREFIX_PATH=/mingw64 \
  -DCMAKE_MAKE_PROGRAM=/mingw64/bin/mingw32-make.exe \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=NEVER \
  -DCMAKE_BUILD_TYPE=Debug

  ==> or Release for the type

cmake --build build -j
./build/ACTA_Gamma.exe
