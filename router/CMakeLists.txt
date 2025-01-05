cmake_minimum_required (VERSION 2.8)

get_filename_component(CUR_DIR ${CMAKE_CURRENT_SOURCE_DIR} NAME)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel/lib/${CUR_DIR})

add_library(router SHARED router.cpp)