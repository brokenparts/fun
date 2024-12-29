cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(Fun_ShapeTest)

set(FUN_NEED_SDL TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../../common/cpp/CMakeLists.txt")

add_executable(shapetest
  "${CMAKE_CURRENT_LIST_DIR}/shapetest.c"
)
target_link_libraries(shapetest PRIVATE fun)