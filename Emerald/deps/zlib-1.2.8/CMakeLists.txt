# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (lib_zlib)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ENDIF(MSVC)

FILE(GLOB Includes"${lib_zlib_SOURCE_DIR}/*.h")
FILE(GLOB Sources "${lib_zlib_SOURCE_DIR}/*.c")

SET(AllSources ${Sources})

ADD_LIBRARY(LIB_ZLIB STATIC ${Sources} ${Includes})
SET_TARGET_PROPERTIES(LIB_ZLIB PROPERTIES LINKER_LANGUAGE C)

SET(LIBRARY_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${Includes})
SOURCE_GROUP ("General sources" FILES ${Sources})