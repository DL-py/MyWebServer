cmake_minimum_required (VERSION 2.8)

project(MyWebServer)

find_package(ZLIB REQUIRED)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/devel)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -ggdb")

find_path(MYSQLCLIENT_INCLUDE NAMES mysql.h PATH_SUFFIXES mysql mariadb)
find_library(MYSQLCLIENT_LIB NAMES mysqlclient mariadb)
if (NOT MYSQLCLIENT_INCLUDE OR NOT MYSQLCLIENT_LIB)
  message(FATAL_ERROR "MySQL client library not found; install default-libmysqlclient-dev")
endif()
include_directories(${MYSQLCLIENT_INCLUDE})

add_subdirectory(src/db)
add_subdirectory(src/auth)
add_subdirectory(src/global)
add_subdirectory(src/logs)
add_subdirectory(src/http_utils)
add_subdirectory(src/http_messages)
add_subdirectory(src/router)
add_subdirectory(src/handler)
add_subdirectory(src/config)
add_subdirectory(src/asset)

add_executable(myhttp src/main.cpp)

target_link_libraries(myhttp  global)
target_link_libraries(myhttp  asset)
target_link_libraries(myhttp  run_log)
target_link_libraries(myhttp  pthread)
target_link_libraries(myhttp  http_utils)
target_link_libraries(myhttp  http_messages)
target_link_libraries(myhttp  router)
target_link_libraries(myhttp  http_hanlder)
target_link_libraries(myhttp  config config++)
target_link_libraries(myhttp  auth)
target_link_libraries(myhttp  db ${MYSQLCLIENT_LIB})
