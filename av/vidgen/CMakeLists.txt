cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(fun_av_vidgen)

set(FUN_NEED_FFMPEG TRUE)
set(FUN_NEED_SDL    TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../../common/cpp/CMakeLists.txt")

add_executable(vidgen
  "${CMAKE_CURRENT_LIST_DIR}/vidgen.cc"
)
target_link_libraries(vidgen PRIVATE fun)