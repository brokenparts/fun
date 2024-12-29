cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(Fun_FFmpeg)

set(FUN_NEED_FFMPEG TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../../common/cpp/CMakeLists.txt")

add_executable(genvid
  "${CMAKE_CURRENT_LIST_DIR}/genvid.c"
)
target_link_libraries(genvid PRIVATE fun)

add_executable(remux
  "${CMAKE_CURRENT_LIST_DIR}/remux.c"
)
target_link_libraries(remux PRIVATE fun)