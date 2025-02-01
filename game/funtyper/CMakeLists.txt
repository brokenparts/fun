cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(fun_game_funtyper)

set(FUN_NEED_FREETYPE TRUE)
set(FUN_NEED_SDL      TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../../common/cpp/CMakeLists.txt")

add_executable(funtyper
  "${CMAKE_CURRENT_LIST_DIR}/funtyper.cc"
)
target_link_libraries(funtyper PRIVATE fun)