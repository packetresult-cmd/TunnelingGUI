# MinGW-w64 크로스 컴파일 툴체인 (Linux/macOS -> Windows)
#
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
#   cmake -S . -B build-x86 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DMINGW_TRIPLET=i686-w64-mingw32
#
# MINGW_TRIPLET: x86_64-w64-mingw32(기본) | i686-w64-mingw32 | aarch64-w64-mingw32 (llvm-mingw)
if(NOT DEFINED MINGW_TRIPLET)
  set(MINGW_TRIPLET x86_64-w64-mingw32)
endif()

set(CMAKE_SYSTEM_NAME Windows)
string(REGEX REPLACE "-.*" "" _arch "${MINGW_TRIPLET}")
set(CMAKE_SYSTEM_PROCESSOR ${_arch})

set(CMAKE_C_COMPILER   ${MINGW_TRIPLET}-gcc)
# The CLI is pure C, but SDL3 enables CXX when the GUI is built. Without this,
# CMake falls back to the host C++ compiler and the link fails on MinGW-only
# options such as --major-image-version.
set(CMAKE_CXX_COMPILER ${MINGW_TRIPLET}-g++)
set(CMAKE_RC_COMPILER  ${MINGW_TRIPLET}-windres)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
