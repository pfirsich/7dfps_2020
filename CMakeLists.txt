if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)
  set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")
endif()

cmake_minimum_required(VERSION 3.0.0)

project(7dfps)

message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

add_compile_definitions(NOMINMAX _USE_MATH_DEFINES) # Windows is trash

add_subdirectory(deps/glwrap)
add_subdirectory(deps/gltf)

# Define functions after add_subdirectory, so they are not overwritten
include(cmake/wall.cmake)

if (7DFPS_ENABLE_ASAN)
  include(cmake/asan.cmake)
endif()

set(SRC
  components.cpp
  ecs.cpp
  gltfimport.cpp
  graphics.cpp
  input.cpp
  main.cpp
  physics.cpp
  util.cpp
)
list(TRANSFORM SRC PREPEND src/)

find_package(fmt CONFIG REQUIRED)

add_executable(7dfps ${SRC})
target_include_directories(7dfps PUBLIC include)
target_include_directories(7dfps PRIVATE deps/glwrap/include)
target_include_directories(7dfps PRIVATE deps/gltf)
target_link_libraries(7dfps PRIVATE glwx gltf)
target_link_libraries(7dfps PRIVATE fmt::fmt)
set_wall(7dfps)
