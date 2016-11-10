cmake_minimum_required(VERSION 2.6)

set(PROJECT_NAME asstk)
project(${PROJECT_NAME})

# assimp
if(NOT DEFINED ASSIMP_DIR)
	message(FATAL_ERROR "Need ASSIMP_DIR definition")
endif()
include_directories(${ASSIMP_DIR}/include)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
set(ASSIMP_BUILD_TESTS OFF)
add_subdirectory(${ASSIMP_DIR} assimp/code)
list(APPEND LINK_LIBS assimp)

# zlib (for assimp)
find_package(zlib REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})
list(APPEND LINK_LIBS ${ZLIB_LIBRARIES})

# imagemagick (for converting textures)
find_package(ImageMagick COMPONENTS Magick++ convert)
message(WARNING "WOAHH ${ImageMagic_INCLUDE_DIRS} ${ImageMagick_LIBARIES} ${ImageMagick_convert_INCLUDE_DIRS}")
include_directories(${ImageMagick_INCLUDE_DIRS})
list(APPEND LINK_LIBS ${ImageMagic_LIBRARIES})

set(SOURCE_DIR ${${PROJECT_NAME}_SOURCE_DIR})
set(CMAKE_MODULE_PATH "${SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH} )

set(BIN_DIR "${SOURCE_DIR}/bin")

#### Application ####

add_definitions("-DDEBUG -DMAGICKCORE_QUANTUM_DEPTH=16 -DMAGICKCORE_HDRI_ENABLE=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -pedantic -std=c++0x -stdlib=libc++")

file(GLOB Source_files *.cpp)

add_executable(${PROJECT_NAME} ${Source_files})

target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${BIN_DIR})
