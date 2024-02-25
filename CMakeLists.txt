cmake_minimum_required (VERSION 2.8)

project(MyWebServer)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel/lib)

add_subdirectory(logs)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -ggdb")

add_executable(myhttp main.cpp http_conn.cpp ConnPool.cpp user_handle.c cgi_exec.c ./xalgorithm/x_file.c)

target_link_libraries(myhttp  run_log)
target_link_libraries(myhttp  pthread mysqlclient)
