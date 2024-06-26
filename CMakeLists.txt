cmake_minimum_required (VERSION 2.8)

project(MyWebServer)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel)

add_subdirectory(logs)
add_subdirectory(xalgorithm)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -ggdb")

add_executable(
        myhttp main.cpp 
        http_conn.cpp 
        ConnPool.cpp 
        user_handle.c 
        http_resp.cpp
        http_req.cpp
        http_com.cpp
        http_handler.cpp
        http_handler_simple.cpp
        http_handler_factory.cpp
        http_handler_cgi.cpp
        )

target_link_libraries(myhttp  run_log)
target_link_libraries(myhttp  pthread mysqlclient)
