cmake_minimum_required (VERSION 2.8)

project(MyWebServer)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -ggdb")

add_subdirectory(global)
add_subdirectory(logs)
add_subdirectory(http_utils)
add_subdirectory(http_messages)
add_subdirectory(router)
add_subdirectory(handler)
add_subdirectory(config)

add_executable(myhttp main.cpp)

target_link_libraries(myhttp  global)
target_link_libraries(myhttp  run_log)
target_link_libraries(myhttp  pthread)
target_link_libraries(myhttp  http_utils)
target_link_libraries(myhttp  http_messages)
target_link_libraries(myhttp  router)
target_link_libraries(myhttp  http_hanlder)
target_link_libraries(myhttp  config config++)
