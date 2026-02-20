# career-coordination-mcp — Multi-Stage Dockerfile
#
# Stage 1 (builder): Ubuntu 22.04 LTS with full build toolchain + vcpkg.
# Stage 2 (runtime): Debian bookworm-slim with only the binary + entrypoint.
#
# The custom vcpkg overlay triplet (docker/triplets/x64-linux-static-release.cmake)
# forces static linkage for all vcpkg packages so the runtime image needs no
# vcpkg-specific shared libraries — only libstdc++6 and libc6 (present in any
# Debian/Ubuntu base image).
#
# Usage:
#   docker build -t ccmcp:latest .
#   docker run --rm -e REDIS_HOST=<host> ccmcp:latest

# ──────────────────────────────────────────────────────────────────────────────
# Stage 1: Builder
# ──────────────────────────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

# Pinned vcpkg commit — must match builtin-baseline in vcpkg.json.
ARG VCPKG_COMMIT=e6ebaa9c3ca8fca90c63af62fc895c2486609580
ARG CMAKE_BUILD_PARALLEL_LEVEL=4

# Disable interactive apt prompts.
ENV DEBIAN_FRONTEND=noninteractive

# Install build toolchain and vcpkg prerequisites.
# Pin packages to avoid non-deterministic installs; use --no-install-recommends
# to keep the builder layer lean.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        git \
        curl \
        zip \
        unzip \
        tar \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Bootstrap vcpkg at the exact pinned commit.
# Metrics are disabled for deterministic, offline-capable builds.
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && cd /opt/vcpkg \
    && git checkout "${VCPKG_COMMIT}" \
    && ./bootstrap-vcpkg.sh -disableMetrics

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="/opt/vcpkg:${PATH}"

# Copy the entire source tree.
WORKDIR /build
COPY . .

# Configure: use vcpkg toolchain + static-linkage overlay triplet + Release build.
# Only the mcp_server target is needed; tests are not built in the container image.
RUN cmake -B build-docker \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_TARGET_TRIPLET=x64-linux-static-release \
        -DVCPKG_OVERLAY_TRIPLETS=/build/docker/triplets \
        -DVCPKG_MANIFEST_MODE=ON

# Build only the server binary (skips tests, CLI tool).
RUN cmake --build build-docker --target mcp_server --parallel "${CMAKE_BUILD_PARALLEL_LEVEL}"

# ──────────────────────────────────────────────────────────────────────────────
# Stage 2: Runtime
# ──────────────────────────────────────────────────────────────────────────────
FROM debian:bookworm-slim AS runtime

# C++ runtime libraries required by statically-linked vcpkg binary.
RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Non-root execution: create a dedicated system user.
# UID/GID 10001 avoids conflicts with common system accounts (0-999) and
# application UIDs (1000-10000).
RUN groupadd -r -g 10001 ccmcp \
    && useradd -r -u 10001 -g ccmcp -s /sbin/nologin -d /home/ccmcp ccmcp \
    && mkdir -p /home/ccmcp \
    && chown ccmcp:ccmcp /home/ccmcp

# Data directory for SQLite database and vector index.
# The volume mount in docker-compose.yml overlays this at runtime.
RUN mkdir -p /data && chown ccmcp:ccmcp /data

# Copy the compiled binary from the builder stage.
COPY --from=builder /build/build-docker/apps/mcp_server/mcp_server /usr/local/bin/mcp_server

# Copy the entrypoint script.
COPY scripts/docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

USER ccmcp
WORKDIR /data

# The entrypoint script reads environment variables, validates required fields,
# and exec-replaces itself with mcp_server + constructed CLI arguments.
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
