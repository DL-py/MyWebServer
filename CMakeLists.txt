cmake_minimum_required (VERSION 2.8)

project(MyWebServer)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

add_subdirectory(logs)

add_executable(myhttp main.cpp http_conn.cpp ConnPool.cpp user_handle.c cgi_exec.c ./xalgorithm/x_file.c)

target_link_libraries(myhttp  run_log)
target_link_libraries(myhttp  pthread mysqlclient)