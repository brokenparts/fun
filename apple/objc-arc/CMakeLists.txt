cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(Fun_ObjcArc)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/cpp/CMakeLists.txt")

add_executable(objc-arc
  "${CMAKE_CURRENT_LIST_DIR}/main.mm"
)
target_compile_options(objc-arc PRIVATE -fobjc-arc)
target_link_libraries(objc-arc PRIVATE "-framework Foundation")
target_link_libraries(objc-arc PRIVATE fun)

add_executable(objc-no-arc
  "${CMAKE_CURRENT_LIST_DIR}/main.mm"
)
target_compile_options(objc-no-arc PRIVATE -fno-objc-arc)
target_link_libraries(objc-no-arc PRIVATE "-framework Foundation")
target_link_libraries(objc-no-arc PRIVATE fun)