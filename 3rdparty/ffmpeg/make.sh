#!/bin/sh

NDK_ROOT=/opt/android-ndk-r8b
$NDK_ROOT/toolchains/arm-linux-androideabi-4.6/prebuilt/linux-x86/bin/arm-linux-androideabi-gcc -fPIC -Wall -shared -o opencv_ffmpeg.so -O2 -x c++ -DANDROID --sysroot=/opt/android-ndk-r8b/platforms/android-8/arch-arm \
    -I$NDK_ROOT/sources/cxx-stl/stlport/stlport -I$NDK_ROOT/platforms/android-8/arch-arm/usr/include -I../include/ffmpeg_ -I../include -I../../modules/highgui/src ffopencv.c -L../lib -lavformat -lavcodec -lavdevice -lswscale -lavutil
