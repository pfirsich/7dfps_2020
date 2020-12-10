if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)
  set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")
endif()

cmake_minimum_required(VERSION 3.0.0)

project(7dfps)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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
  client.cpp
  components.cpp
  ecs.cpp
  enet.cpp
  gltfimport.cpp
  graphics.cpp
  input.cpp
  main.cpp
  net.cpp
  physics.cpp
  random.cpp
  serialization.cpp
  server.cpp
  shipsystem.cpp
  util.cpp
)
list(TRANSFORM SRC PREPEND src/)

find_package(fmt CONFIG REQUIRED)
find_package(ENet REQUIRED)
find_package(Threads REQUIRED)
find_package(docopt COMPONENTS CXX REQUIRED)

if (NOT WIN32)
  find_library(LUAJIT_LIBRARY_DIR libluajit-5.1.a REQUIRED)
  find_path(LUAJIT_INCLUDE_DIR luajit.h PATH_SUFFIXES luajit-2.1 REQUIRED)
  add_library(luajit INTERFACE)
  target_include_directories(luajit INTERFACE ${LUAJIT_INCLUDE_DIR})
  target_link_libraries(luajit INTERFACE ${LUAJIT_LIBRARY_DIR})
  message(STATUS "LuaJIT Include Dir: ${LUAJIT_INCLUDE_DIR}")
  message(STATUS "LuaJIT Library Dir: ${LUAJIT_LIBRARY_DIR}")
endif()

add_executable(7dfps ${SRC})
target_include_directories(7dfps PUBLIC include)
target_include_directories(7dfps PRIVATE deps/glwrap/include)
target_include_directories(7dfps PRIVATE deps/gltf)
target_include_directories(7dfps PRIVATE ${ENET_INCLUDE_DIRS})
target_include_directories(7dfps PRIVATE ${DOCOPT_INCLUDE_DIRS})
target_include_directories(7dfps SYSTEM PRIVATE deps/sol2/single/include)
target_link_libraries(7dfps PRIVATE glwx gltf)
target_link_libraries(7dfps PRIVATE fmt::fmt)
target_link_libraries(7dfps PRIVATE ${ENET_LIBRARIES})
target_link_libraries(7dfps PRIVATE Threads::Threads)
target_link_libraries(7dfps PRIVATE docopt)
target_link_libraries(7dfps PRIVATE luajit)

set_wall(7dfps)
