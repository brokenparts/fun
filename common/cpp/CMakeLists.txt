add_library(fun INTERFACE)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

#
# Common headers
#
target_include_directories(fun INTERFACE
  "${CMAKE_CURRENT_LIST_DIR}"
)

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
# Dependency: SDL
# https://www.libsdl.org/
#
if(FUN_NEED_SDL)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vendor/SDL" SDL)
  target_link_libraries(fun INTERFACE SDL3::SDL3)
endif()