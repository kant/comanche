cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

include($ENV{COMANCHE_HOME}/mk/common.cmake)

#add_subdirectory(./unit_test)

project(mddb C CXX)

include_directories(../../components/)
include_directories(../../lib/common/include)
include_directories(../../lib/core/include)
link_directories(../../lib/common)
link_directories(../../../lib)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)

file(GLOB SOURCES src/*.c*)

set(CMAKE_SHARED_LINKER_FLAGS "-fPIC -shared -Wl,--no-undefined")

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_link_libraries(${PROJECT_NAME} comanche-core numa pthread common dl rt sqlite3)


