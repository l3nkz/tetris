project(tetris)

cmake_minimum_required(VERSION 3.1)

# general setup
set(LIBRARY_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin")

# use c++17
set(CMAKE_CXX_STANDARD "17")

# dependent libraries
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

# libtetris client version
add_library(tetrisclient SHARED tetris_client.cc debug_util.cc)
target_link_libraries(tetrisclient Threads::Threads ${CMAKE_DL_LIBS})

# tetris server binary
add_executable(tetrisserver tetris_server.cc algorithm.cc equivalence.cc debug_util.cc)

# tetris control binary
add_executable(tetrisctl tetris_ctl.cc)
