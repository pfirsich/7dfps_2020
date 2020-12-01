cmake_minimum_required(VERSION 3.0.0)

project(7dfps)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(cmake/wall.cmake)

if (7dfps_ENABLE_ASAN)
  include(cmake/asan.cmake)
endif()

set(SRC
    main.cpp
    ecs.cpp
)
list(TRANSFORM SRC PREPEND src/)

add_subdirectory(deps/glwrap)
add_subdirectory(deps/gltf)
add_subdirectory(deps/fmt)

add_executable(7dfps ${SRC})
target_include_directories(7dfps PUBLIC include)
target_include_directories(7dfps PRIVATE deps/glwrap/include)
target_include_directories(7dfps PRIVATE deps/gltf)
target_link_libraries(7dfps glwx gltf)
target_link_libraries(7dfps fmt)
set_wall(7dfps)
