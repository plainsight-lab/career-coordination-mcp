# Repository-local helper toolchain for vcpkg integration.
# Usage:
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/vcpkg.cmake

if(DEFINED ENV{VCPKG_ROOT})
  set(_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
elseif(EXISTS "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
  set(_VCPKG_ROOT "$ENV{HOME}/vcpkg")
else()
  message(FATAL_ERROR "VCPKG_ROOT is not set and ~/vcpkg was not found")
endif()

set(VCPKG_TARGET_TRIPLET "arm64-osx" CACHE STRING "vcpkg target triplet")
include("${_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
