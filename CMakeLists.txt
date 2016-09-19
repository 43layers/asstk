cmake_minimum_required(VERSION 2.6)

set(PROJECT_NAME asstk)
project(${PROJECT_NAME})

# assimp
if(NOT DEFINED ASSIMP_DIR)
	message(FATAL_ERROR "Need ASSIMP_DIR definition")
endif()
include_directories(${ASSIMP_DIR}/include)
list(APPEND LINK_LIBS ${ASSIMP_DIR}/lib/libassimp.a)

# zlib (for assimp)
find_package(zlib REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})
list(APPEND LINK_LIBS ${ZLIB_LIBRARIES})


set(SOURCE_DIR ${${PROJECT_NAME}_SOURCE_DIR})
set(CMAKE_MODULE_PATH "${SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH} )

set(BIN_DIR "${SOURCE_DIR}/bin")

#### Application ####

add_definitions("-D DEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wconversion -Wextra -pedantic -std=c++0x -stdlib=libc++")

file(GLOB Source_files *.cpp)

add_executable(${PROJECT_NAME} ${Source_files})

target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${BIN_DIR})
