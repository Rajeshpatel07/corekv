# Build stage
FROM gcc:13 AS build
WORKDIR /src
COPY bench/corekv_bench.cpp .
RUN g++ -O3 -std=c++17 -pthread corekv_bench.cpp -o /usr/local/bin/corekv-bench

# Runtime stage (see server.Dockerfile for the libstdc++ version rationale)
FROM ubuntu:24.04 AS runtime
COPY --from=build /usr/local/bin/corekv-bench /usr/local/bin/corekv-bench
EXPOSE 9091
ENTRYPOINT ["corekv-bench"]
