FROM python:3.9-bookworm AS builder

WORKDIR /app
COPY conanfile.py CMakeLists.txt ./

RUN pip install conan
RUN apt-get update && apt-get install -y --no-install-recommends cmake

RUN conan profile detect
RUN conan install . --output-folder=build --build=missing

COPY src src/
RUN cd /app/build && cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release && make

FROM debian:bookworm-slim AS final

COPY --from=builder /app/build/Raito-Server /app/build/Raito-Server
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates

WORKDIR /app/build
RUN chmod +x Raito-Server

EXPOSE 8000
CMD [ "stdbuf", "-oL", "sh", "-c", "/app/build/Raito-Server" ]
