set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

cmake_minimum_required(VERSION 3.0.0)

project(7dfps)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

add_compile_definitions(NOMINMAX _USE_MATH_DEFINES) # Windows is trash

include(cmake/wall.cmake)

if (7dfps_ENABLE_ASAN)
  include(cmake/asan.cmake)
endif()

set(SRC
  ecs.cpp
  input.cpp
  main.cpp
)
list(TRANSFORM SRC PREPEND src/)

add_subdirectory(deps/glwrap)
add_subdirectory(deps/gltf)

find_package(fmt CONFIG REQUIRED)

add_executable(7dfps ${SRC})
target_include_directories(7dfps PUBLIC include)
target_include_directories(7dfps PRIVATE deps/glwrap/include)
target_include_directories(7dfps PRIVATE deps/gltf)
target_link_libraries(7dfps PRIVATE glwx gltf)
target_link_libraries(7dfps PRIVATE fmt::fmt)
set_wall(7dfps)
