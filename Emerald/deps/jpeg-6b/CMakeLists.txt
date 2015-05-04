# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (lib_jpeg)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

FILE(GLOB Includes "${lib_jpeg_SOURCE_DIR}/jconfig.h")
FILE(GLOB Sources  "${lib_jpeg_SOURCE_DIR}/jcomapi.c"
                   "${lib_jpeg_SOURCE_DIR}/jd*.c"
                   "${lib_jpeg_SOURCE_DIR}/jidct*.c"
                   "${lib_jpeg_SOURCE_DIR}/jerror.c"
                   "${lib_jpeg_SOURCE_DIR}/jmemmgr.c"
                   "${lib_jpeg_SOURCE_DIR}/jmemnobs.c"
                   "${lib_jpeg_SOURCE_DIR}/jquant1.c"
                   "${lib_jpeg_SOURCE_DIR}/jquant2.c"
                   "${lib_jpeg_SOURCE_DIR}/jutils.c")

SET(AllSources ${Sources})

ADD_LIBRARY(lib_jpeg STATIC ${Sources} ${Includes})
SET_TARGET_PROPERTIES(lib_jpeg PROPERTIES LINKER_LANGUAGE C)

SET(LIBRARY_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${Includes})
SOURCE_GROUP ("General sources" FILES ${Sources})