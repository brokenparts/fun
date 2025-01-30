cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(fun_av_vidshader)

set(FUN_NEED_FFMPEG TRUE)
set(FUN_NEED_GLAD33 TRUE)
set(FUN_NEED_SDL    TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../../common/cpp/CMakeLists.txt")

add_executable(vidshader
  "${CMAKE_CURRENT_LIST_DIR}/vidshader.cc"
)
target_link_libraries(vidshader PRIVATE fun)