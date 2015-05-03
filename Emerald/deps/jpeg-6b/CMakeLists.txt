# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (lib_jpeg)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

FILE(GLOB Includes"${lib_jpeg_BINARY_DIR}/jconfig.h")
FILE(GLOB Sources "${lib_jpeg_BINARY_DIR}/jcomapi.c"
                  "${lib_jpeg_BINARY_DIR}/jd*.c"
                  "${lib_jpeg_BINARY_DIR}/jidct*.c"
                  "${lib_jpeg_BINARY_DIR}/jerror.c"
                  "${lib_jpeg_BINARY_DIR}/jmemmgr.c"
                  "${lib_jpeg_BINARY_DIR}/jmemnobs.c"
                  "${lib_jpeg_BINARY_DIR}/jquant1.c"
                  "${lib_jpeg_BINARY_DIR}/jquant2.c"
                  "${lib_jpeg_BINARY_DIR}/jutils.c")

SET(AllSources ${Sources})

ADD_LIBRARY(LIB_JPEG STATIC ${Sources} ${Includes})

SET(LIBRARY_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${Includes})
SOURCE_GROUP ("General sources" FILES ${Sources})