cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(comanche-raid CXX)
enable_language(CXX C ASM)

add_subdirectory(./unit_test)

include($ENV{COMANCHE_HOME}/mk/common.cmake)

include_directories(./src)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)

file(GLOB SOURCES ./src/*.c*)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

target_link_libraries(${PROJECT_NAME} ${ASAN_LIB} common comanche-core numa pthread dl rt)


