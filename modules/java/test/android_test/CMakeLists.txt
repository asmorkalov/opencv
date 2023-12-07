project(opencv_test_java)

set(OPENCV_JAVA_TEST_DIR "${OpenCV_BINARY_DIR}/opencv_android/android_test" CACHE INTERNAL "")
file(REMOVE_RECURSE "${OPENCV_JAVA_TEST_DIR}")
file(REMOVE "${OPENCV_DEPHELPER}/${the_module}_test_source_copy")

set(test_dir "${CMAKE_CURRENT_SOURCE_DIR}")

set(depends "")

# 1. gather and copy common test files (resources, utils, etc.)
copy_common_tests("${CMAKE_CURRENT_SOURCE_DIR}/../common_test" "${OPENCV_JAVA_TEST_DIR}" depends)

# 2. gather and copy tests from each module
ocv_copyfiles_append_dir(JAVA_TEST_SRC_COPY "${OPENCV_JAVA_BINDINGS_DIR}/gen/test" "${OPENCV_JAVA_TEST_DIR}/src")

list(APPEND depends gen_opencv_java_source "${OPENCV_DEPHELPER}/gen_opencv_java_source")
ocv_copyfiles_add_target(${the_module}_test_source_copy JAVA_TEST_SRC_COPY "Copy Java(Android test) source files" ${depends})
set(depends ${the_module}_test_source_copy "${OPENCV_DEPHELPER}/${the_module}_test_source_copy")

# 3. gather and copy specific files for Android
file(GLOB_RECURSE test_files RELATIVE "${test_dir}" "${test_dir}/res/*" "${test_dir}/src/*")
foreach(f ${test_files} ${ANDROID_MANIFEST_FILE})
  add_custom_command(
      OUTPUT "${OPENCV_JAVA_TEST_DIR}/${f}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${test_dir}/${f}" "${OPENCV_JAVA_TEST_DIR}/${f}"
      MAIN_DEPENDENCY "${test_dir}/${f}"
      COMMENT "Copying ${f}")
  list(APPEND depends "${test_dir}/${f}" "${OPENCV_JAVA_TEST_DIR}/${f}")
endforeach()

list(APPEND depends ${android_proj_target_files})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/build.gradle.in" "${OPENCV_JAVA_TEST_DIR}/build.gradle" @ONLY)

add_custom_command(
    OUTPUT "${OPENCV_JAVA_TEST_DIR}/bin/OpenCVTest-debug.apk"
    #COMMAND ./gradlew ${OPENCV_GRADLE_VERBOSE_OPTIONS} "opencv_test:assemble"
    COMMAND ${CMAKE_COMMAND} -E touch "${OPENCV_JAVA_TEST_DIR}/bin/OpenCVTest-debug.apk"
    WORKING_DIRECTORY "${OPENCV_JAVA_TEST_DIR}"
    MAIN_DEPENDENCY "${OPENCV_JAVA_TEST_DIR}/${ANDROID_MANIFEST_FILE}"
    DEPENDS opencv_java_android opencv_java
    DEPENDS ${depends}
    COMMENT "Building OpenCV Android library project")

add_custom_target(${PROJECT_NAME} ALL SOURCES "${OPENCV_JAVA_TEST_DIR}/bin/OpenCVTest-debug.apk" "${CMAKE_CURRENT_SOURCE_DIR}/${ANDROID_MANIFEST_FILE}")
add_dependencies(opencv_tests ${PROJECT_NAME})
