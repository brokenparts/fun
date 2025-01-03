add_library(fun INTERFACE)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Export DLLs to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

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
  target_compile_options(fun INTERFACE -fsanitize=address -g)
  target_link_options(fun INTERFACE -fsanitize=address -g)
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
# Dependency: glad (3.3)
# https://glad.dav1d.de/
#
if(FUN_NEED_GLAD33)
  add_library(fun_glad33
    "${CMAKE_CURRENT_LIST_DIR}/vendor/glad33/src/glad.c"
  )
  target_include_directories(fun_glad33 PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/vendor/glad33/include"
  )
  target_link_libraries(fun INTERFACE fun_glad33)
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
  set(FUN_NEED_ZLIB TRUE)
  add_library(fun_spng
    "${CMAKE_CURRENT_LIST_DIR}/vendor/libspng/spng/spng.c"
  )
  target_include_directories(fun_spng PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/vendor/libspng/spng"
  )
  target_compile_definitions(fun_spng PUBLIC SPNG_STATIC)
  target_link_libraries(fun_spng PRIVATE zlib)
  target_link_libraries(fun INTERFACE fun_spng)
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
endif()

#
# Dependency: SDL
# https://www.libsdl.org/
#
if(FUN_NEED_SDL)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vendor/SDL" SDL)
  target_link_libraries(fun INTERFACE SDL3::SDL3)
endif()

#
# Dependency: zlib-ng
# https://github.com/zlib-ng/zlib-ng
#
if(FUN_NEED_ZLIB)
  set(ZLIB_COMPAT TRUE)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vendor/zlib-ng" zlib)
endif()
