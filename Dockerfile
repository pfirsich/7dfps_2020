##
## Build Env
##

FROM ubuntu AS builder

WORKDIR /usr/src/app

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
	apt install --assume-yes \
		build-essential ninja-build cmake clang-10 libsdl2-dev \
		libfmt-dev libglm-dev libenet-dev libdocopt-dev

COPY deps deps
COPY cmake cmake
COPY tests tests
COPY other other
COPY media media
COPY src src

COPY CMakeLists.txt CMakeLists.txt

RUN mkdir -p build && cd build && \
	cmake -G Ninja -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo -DCMAKE_CXX_COMPILER=clang++-10 ..

RUN cmake --build build --parallel && \
	cd ..

# Clean up
RUN rm -rf deps cmake test other src ./build/deps ./build/CMakeFiles
