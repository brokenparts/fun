cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(Fun_Bvh)

set(FUN_NEED_SDL TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../../common/cpp/CMakeLists.txt")

add_executable(bvh
  "${CMAKE_CURRENT_LIST_DIR}/bvh.cc"
)
target_link_libraries(bvh PRIVATE fun)