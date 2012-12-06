project(opencv_ffmpeg)

include_directories(${FFMPEG_INCLUDE_DIR} ../include ../../modules/highgui/src)
link_directories(${FFMPEG_LIB_DIR})
add_library(opencv_ffmpeg SHARED ffopencv.c)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -x c++")
target_link_libraries(opencv_ffmpeg avformat avcodec swscale avutil)
install(FILES "${LIBRARY_OUTPUT_PATH}/opencv_ffmpeg" DESTINATION ${OPENCV_LIB_INSTALL_PATH} COMPONENT main)