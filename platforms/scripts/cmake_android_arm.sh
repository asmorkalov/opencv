#!/bin/sh
cd `dirname $0`/..

mkdir -p build_android_arm
cd build_android_arm

cmake -DANDROID_ABI="arm64-v8a" -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON -DCMAKE_TOOLCHAIN_FILE=../android/android.toolchain.cmake $@ ../..
