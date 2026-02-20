# vcpkg overlay triplet: x64-linux-static-release
#
# Forces static linkage for all vcpkg-managed packages so the runtime image
# needs only libstdc++6 + libc6 (both present in any debian:bookworm-slim base).
# Release-only builds keep the image layer smaller.

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CXX_FLAGS "")
set(VCPKG_C_FLAGS "")
set(VCPKG_BUILD_TYPE release)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
