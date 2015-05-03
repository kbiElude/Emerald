# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (LW-AnimationDataExtractor) 

SET(CMAKE_INCLUDE_PATH "${LW-AnimationDataExtractor_BINARY_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
INCLUDE_DIRECTORIES("${LW-AnimationDataExtractor_BINARY_DIR}")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/include")

FILE(GLOB GeneralIncludes "${LW-AnimationDataExtractor_BINARY_DIR}/*.h")
FILE(GLOB GeneralSources "${LW-AnimationDataExtractor_BINARY_DIR}/*.cc")

SET(AllLW-AnimationDataExtractorSources ${GeneralIncludes} ${GeneralSources} )

set (CMAKE_LINK_DEF_FILE_FLAG "/DEF:serv.def") 

ADD_LIBRARY(LW-AnimationDataExtractor SHARED ${AllLW-AnimationDataExtractorSources} ${GeneralIncludes})
SET_TARGET_PROPERTIES(LW-AnimationDataExtractor PROPERTIES LINK_FLAGS "/MANIFEST:NO")

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(LW-AnimationDataExtractor EMERALD)
ADD_DEPENDENCIES(LW-AnimationDataExtractor EMERALD)