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
# Dependency: FreeType
# https://freetype.org/
#
if(FUN_NEED_FREETYPE)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/thirdparty/freetype" freetype)
  target_link_libraries(fun INTERFACE freetype)
endif()

#
# Dependency: Glad (3.3)
# https://glad.dav1d.de/
#
if(FUN_NEED_GLAD33)
  add_library(fun_glad33 STATIC
    "${CMAKE_CURRENT_LIST_DIR}/thirdparty/glad33/src/glad.c"
  )
  target_include_directories(fun_glad33 PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/thirdparty/glad33/include"
  )
  target_link_libraries(fun INTERFACE fun_glad33)
endif()

#
# Dependency: SDL
# https://www.libsdl.org/
#
if(FUN_NEED_SDL)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/thirdparty/SDL" SDL)
  target_link_libraries(fun INTERFACE SDL3::SDL3)
endif()

