#!/bin/sh
cd `dirname $0`/..

mkdir -p build_ffmpeg
cd build_ffmpeg

export FFMPEG_LIB_DIR=~/Projects/ffmpeg-1.0/build/ffmpeg/lib
export FFMPEG_INCLUDE_DIR=~/Projects/ffmpeg-1.0/build/ffmpeg/include

cmake -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON -DBUILD_PERF_TESTS=OFF -DBUILD_TESTS=OFF -DWITH_FFMPEG=ON -DFFMPEG_LIB_DIR=$FFMPEG_LIB_DIR -DFFMPEG_INCLUDE_DIR=$FFMPEG_INCLUDE_DIR -DBUILD_SHARED_LIBS=ON -DCMAKE_TOOLCHAIN_FILE=../android.toolchain.cmake $@ ../..

