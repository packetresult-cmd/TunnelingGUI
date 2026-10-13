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
set(CMAKE_RC_COMPILER  ${MINGW_TRIPLET}-windres)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
