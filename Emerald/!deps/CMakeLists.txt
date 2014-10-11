# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 

ADD_SUBDIRECTORY(jpeg-6b)
ADD_SUBDIRECTORY(libpng-1.2.36)
ADD_SUBDIRECTORY(tinyxml2)
ADD_SUBDIRECTORY(zlib-1.2.3)
