cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(Fun_ObjcArc)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/cpp/CMakeLists.txt")

add_executable(touch
  "${CMAKE_CURRENT_LIST_DIR}/main.mm"
)
target_compile_options(touch PRIVATE
	-Wno-missing-declarations
)
target_link_options(touch PRIVATE
	-F/System/Library/PrivateFrameworks
)
target_link_libraries(touch PRIVATE
	"-framework Foundation"
	"-framework Cocoa"
	"-framework CoreGraphics"
	"-framework MultitouchSupport"
)
target_link_libraries(touch PRIVATE fun)
