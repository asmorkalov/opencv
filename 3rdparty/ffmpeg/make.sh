#!/bin/sh

export PATH=/opt/android-ndk-r8b/toolchains/arm-linux-androideabi-4.6/prebuilt/linux-x86/bin:$PATH
arm-linux-androideabi-gcc -Wall -shared -o opencv_ffmpeg.so -O2 -x c++ -DANDROID -I/opt/android-ndk-r8b/sources/cxx-stl/stlport/stlport -I/opt/android-ndk-r8b/platforms/android-8/arch-arm/usr/include -I../include/ffmpeg_ -I../include -I../../modules/highgui/src ffopencv.c -L../lib -lavformat -lavcodec -lavdevice -lswscale -lavutil
