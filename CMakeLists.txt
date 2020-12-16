if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)
  set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")
endif()

cmake_minimum_required(VERSION 3.0.0)

project(complexity)
set(APP_NAME complexity)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

add_compile_definitions(NOMINMAX _USE_MATH_DEFINES) # Windows is trash

add_subdirectory(deps/glwrap)
add_subdirectory(deps/gltf)

# Define functions after add_subdirectory, so they are not overwritten
include(cmake/wall.cmake)

if (COMPLEXITY_ENABLE_ASAN)
  include(cmake/asan.cmake)
endif()

set(SRC
  client.cpp
  components.cpp
  ecs.cpp
  enet.cpp
  gltfimport.cpp
  graphics.cpp
  imgui.cpp
  input.cpp
  main.cpp
  net.cpp
  physics.cpp
  random.cpp
  serialization.cpp
  server.cpp
  shipsystem.cpp
  sound.cpp
  util.cpp
)
list(TRANSFORM SRC PREPEND src/)

set(IMGUI_SRC
  backends/imgui_impl_opengl3.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
  imgui.cpp
  misc/cpp/imgui_stdlib.cpp
)
list(TRANSFORM IMGUI_SRC PREPEND deps/imgui/)
list(APPEND IMGUI_SRC src/imgui_impl_sdl.cpp)
list(APPEND SRC ${IMGUI_SRC})

find_package(fmt CONFIG REQUIRED)
find_package(ENet REQUIRED)
find_package(Threads REQUIRED)
find_package(docopt COMPONENTS CXX REQUIRED)

find_library(LUAJIT_LIBRARY_DIR libluajit-5.1.a lua51 REQUIRED)
find_path(LUAJIT_INCLUDE_DIR luajit.h PATH_SUFFIXES luajit-2.1 luajit EQUIRED)
add_library(luajit INTERFACE)
target_include_directories(luajit INTERFACE ${LUAJIT_INCLUDE_DIR})
target_link_libraries(luajit INTERFACE ${LUAJIT_LIBRARY_DIR})
message(STATUS "LuaJIT Include Dir: ${LUAJIT_INCLUDE_DIR}")
message(STATUS "LuaJIT Library Dir: ${LUAJIT_LIBRARY_DIR}")

add_subdirectory(deps/soloud)

# add_executable(complexity ${SRC})
add_executable(complexity MACOSX_BUNDLE ${SRC})

target_include_directories(complexity PUBLIC include)
target_include_directories(complexity PRIVATE deps/glwrap/include)
target_include_directories(complexity PRIVATE deps/gltf)
target_include_directories(complexity PRIVATE ${ENET_INCLUDE_DIRS})
target_include_directories(complexity PRIVATE ${DOCOPT_INCLUDE_DIRS})
target_include_directories(complexity SYSTEM PRIVATE deps/sol2/single/include)
target_include_directories(complexity SYSTEM PRIVATE deps/imgui deps/imgui/backends)
target_include_directories(complexity SYSTEM PRIVATE deps/imgui deps/soloud/include)
target_link_libraries(complexity PRIVATE glwx gltf)
target_link_libraries(complexity PRIVATE fmt::fmt)
target_link_libraries(complexity PRIVATE ${ENET_LIBRARIES})
target_link_libraries(complexity PRIVATE Threads::Threads)
target_link_libraries(complexity PRIVATE docopt)
target_link_libraries(complexity PRIVATE luajit)
target_link_libraries(complexity PRIVATE soloud)

set_wall(complexity)

# Destination paths below are relative to ${CMAKE_INSTALL_PREFIX}
install(TARGETS ${APP_NAME}
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
    )

# Note Mac specific extension .app
set(APPS "\${CMAKE_INSTALL_PREFIX}/${APP_NAME}.app")

# Directories to look for dependencies
set(DIRS ${CMAKE_BINARY_DIR})

install(CODE "include(BundleUtilities)
    fixup_bundle(\"${APPS}\" \"\" \"${DIRS}\")")

set(CPACK_GENERATOR "DRAGNDROP")
include(CPack)
