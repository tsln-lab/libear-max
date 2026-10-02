# Copyright (c) 2026 tsln-lab. Use of this source code is governed by the MIT License found in the License.md file.

cmake_minimum_required(VERSION 3.19)

set(C74_MIN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../min-api)
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

#############################################################
# MAX EXTERNAL
#############################################################

include_directories("${C74_INCLUDES}")

set(SOURCE_FILES
    ${PROJECT_NAME}.cpp
    ${PROJECT_NAME}.h
    ../shared/ear_max.h
    ../shared/ear_max_adm.h
    ../shared/ear_max_adm_capture.h
    ../shared/ear_max_adm_player.h
    ../shared/ear_max_direct.h
    ../shared/ear_max_sink.h
    ../shared/ear_max_mc.h
)

add_library(${PROJECT_NAME} MODULE ${SOURCE_FILES})
# libadm (static) and libbw64 (header-only) are built from the submodules by the root CMakeLists.txt
target_link_libraries(${PROJECT_NAME} PRIVATE ear adm bw64)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)

#############################################################
# UNIT TEST
#############################################################

include(${C74_MIN_API_DIR}/test/min-object-unittest.cmake)
if (TARGET ${PROJECT_NAME}_test)
    target_link_libraries(${PROJECT_NAME}_test PRIVATE ear adm bw64 Threads::Threads)
    # the tests regenerate the reference page into the package's docs folder (see ear_max_test.h)
    target_compile_definitions(${PROJECT_NAME}_test PRIVATE
        EARMAX_DOCS_DIR="${CMAKE_SOURCE_DIR}/docs"
        # the parity fixture of ear.adm and a folder for the files the tests write
        EARMAX_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../ear.adm/test_data"
        EARMAX_TEST_OUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
endif ()
