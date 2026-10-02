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
    ../shared/ear_max_dsp.h
    ../shared/ear_max_mc.h
)

add_library(${PROJECT_NAME} MODULE ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE ear)

include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)

#############################################################
# UNIT TEST
#############################################################

include(${C74_MIN_API_DIR}/test/min-object-unittest.cmake)
if (TARGET ${PROJECT_NAME}_test)
    target_link_libraries(${PROJECT_NAME}_test PRIVATE ear)
    # the tests regenerate the reference page into the package's docs folder (see ear_max_test.h)
    target_compile_definitions(${PROJECT_NAME}_test PRIVATE EARMAX_DOCS_DIR="${CMAKE_SOURCE_DIR}/docs")
endif ()
