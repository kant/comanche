cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

include($ENV{COMANCHE_HOME}/mk/common.cmake)

add_subdirectory(./unit_test)

project(comanche-rdma CXX)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)

file(GLOB SOURCES src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

target_link_libraries(${PROJECT_NAME} common comanche-core pthread numa dl rt z rdmacm ibverbs)


