# The server links statically and reads nothing off disk, so the runtime image
# can be empty - no distro, no libc, just the binary. Around 1MB.

FROM debian:bookworm-slim AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ cmake make \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY src ./src
COPY tests ./tests

# BUILD_CLIENT=OFF keeps SFML out of it entirely. The tests run here, so a
# broken build never becomes an image.
RUN cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_CLIENT=OFF \
        -DSTATIC_SERVER=ON \
 && cmake --build build -j"$(nproc)" \
 && ctest --test-dir build --output-on-failure \
 && strip build/server

FROM scratch
COPY --from=build /src/build/server /server
USER 65534:65534
EXPOSE 9422/udp
ENTRYPOINT ["/server"]
CMD ["9422"]
