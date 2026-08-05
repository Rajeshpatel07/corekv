# Build stage
FROM gcc:13 AS build
WORKDIR /src
COPY . .
# gcc:13 ships Debian bookworm (cmake 3.25); the project needs 3.31+,
# so install a current CMake binary from PyPI.
RUN apt-get update \
    && apt-get install -y --no-install-recommends python3-pip \
    && pip3 install --break-system-packages --no-cache-dir cmake \
    && rm -rf /var/lib/apt/lists/* \
    && cmake --version | head -1
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DCOREKV_BUILD_BENCH=OFF \
    && cmake --build build -j$(nproc)

# Runtime stage. corekv-server is compiled with GCC 13's libstdc++ (needs
# GLIBCXX_3.4.32); the gcc:13 image is bookworm-based and only ships 6.0.30,
# so we run on ubuntu:24.04 (GCC 13.2) which provides libstdc++ 6.0.32.
FROM ubuntu:24.04 AS runtime
RUN apt-get update \
    && apt-get install -y --no-install-recommends bash \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/bin/corekv-server /usr/local/bin/corekv-server
EXPOSE 8000
HEALTHCHECK --interval=5s --timeout=2s --start-period=3s --retries=3 \
    CMD bash -c 'exec 3<>/dev/tcp/127.0.0.1/8000 && exec 3>&-' || exit 1
CMD ["corekv-server"]
