cmake_minimum_required (VERSION 2.8)

get_filename_component(CUR_DIR ${CMAKE_CURRENT_SOURCE_DIR} NAME)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel/lib/${CUR_DIR})

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -ggdb")

add_library(http_messages SHARED http_request_method.cpp http_request.cpp http_response_code.cpp http_response.cpp)