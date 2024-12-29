add_library(fun INTERFACE)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

#
# Common headers
#
target_include_directories(fun INTERFACE
  "${CMAKE_CURRENT_LIST_DIR}"
)

# Add homebrew libraries to search path
if(APPLE)
  target_include_directories(fun INTERFACE
    "/opt/homebrew/include"
  )
  target_link_directories(fun INTERFACE
    "/opt/homebrew/lib"
  )
endif()

#
# Enable compiler warnings
#
if(MSVC)
# @@
else()
  target_compile_options(fun INTERFACE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

#
# Enable AddressSanitizer
#
if(NOT MSVC)
  target_compile_options(fun INTERFACE -fsanitize=address)
  target_link_options(fun INTERFACE -fsanitize=address)
endif()

# Use C++20
target_compile_features(fun INTERFACE cxx_std_20)

#
# Dependency: cgltf
# https://github.com/jkuhlmann/cgltf
#
if(FUN_NEED_CGLTF)
  add_library(fun_cgltf
    "${CMAKE_CURRENT_LIST_DIR}/vendor/cgltf_impl.c"
  )
  target_include_directories(fun_cgltf PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/vendor/cgltf"
  )
  target_link_libraries(fun INTERFACE fun_cgltf)
endif()

#
# Dependency: FFmpeg
# https://www.ffmpeg.org
#
if(FUN_NEED_FFMPEG)
  target_link_libraries(fun INTERFACE
    avcodec
    avformat
    avutil
    swscale
  )
endif()

#
# Dependency: Handmade math
# https://github.com/HandmadeMath/HandmadeMath
#
if(FUN_NEED_HANDMADE_MATH)
  target_include_directories(fun INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/vendor/HandmadeMath"
  )
endif()

#
# Dependency: libspng
# https://libspng.org/
#
if(FUN_NEED_LIBSPNG)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vendor/libspng" spng)
  target_link_libraries(fun INTERFACE spng)
endif()

#
# Dependency: Metal-cpp
# https://developer.apple.com/metal/cpp/
#
if(FUN_NEED_METAL_CPP)
  target_include_directories(fun INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/vendor/metal-cpp"
  )
  target_link_libraries(fun INTERFACE
    objc
    "-framework Foundation"
    "-framework Metal"
  )
  # target_link_libraries(fun INTERFACE
  #   "-framework Metal"
  #   "-framework MetalKit"
  # )
endif()

#
# Dependency: SDL
# https://www.libsdl.org/
#
if(FUN_NEED_SDL)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vendor/SDL" SDL)
  target_link_libraries(fun INTERFACE SDL3::SDL3)
endif()