cmake_minimum_required (VERSION 2.8)

project(MyWebServer)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel)

add_subdirectory(logs)
add_subdirectory(xalgorithm)
add_subdirectory(http_utils)
add_subdirectory(http_messages)
add_subdirectory(router)
add_subdirectory(handler)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -ggdb")

# add_executable(
#         myhttp main.cpp 
#         http_conn.cpp 
#         ConnPool.cpp 
#         user_handle.c 
#         http_resp.cpp
#         http_req.cpp
#         http_com.cpp
#         http_handler.cpp
#         http_handler_simple.cpp
#         http_handler_factory.cpp
#         http_handler_cgi.cpp
#         )

add_executable(myhttp main.cpp)

target_link_libraries(myhttp  run_log)
target_link_libraries(myhttp  pthread mysqlclient)
target_link_libraries(myhttp  http_utils)
target_link_libraries(myhttp  http_messages)
target_link_libraries(myhttp  router)
target_link_libraries(myhttp  http_hanlder)
