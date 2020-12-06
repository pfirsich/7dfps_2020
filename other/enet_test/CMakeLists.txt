cmake_minimum_required(VERSION 3.0.0)

project(enet-test)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(cmake/wall.cmake)

if (ENET_TEST_ENABLE_ASAN)
  include(cmake/asan.cmake)
endif()

set(SRC
    main.cpp
)
list(TRANSFORM SRC PREPEND src/)

find_package(ENet REQUIRED)

add_executable(enet-test ${SRC})
target_include_directories(enet-test PRIVATE ${ENET_INCLUDE_DIRS})
target_link_libraries(enet-test ${ENET_LIBRARIES})
set_wall(enet-test)
