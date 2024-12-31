cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(Fun_OpenGL)

set(FUN_NEED_CGLTF          TRUE)
set(FUN_NEED_GLAD33         TRUE)
set(FUN_NEED_HANDMADE_MATH  TRUE)
set(FUN_NEED_LIBSPNG        TRUE)
set(FUN_NEED_SDL            TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../../common/cpp/CMakeLists.txt")

add_executable(opengl_gltf
  "${CMAKE_CURRENT_LIST_DIR}/gltf.cc"
)
target_link_libraries(opengl_gltf PRIVATE fun)