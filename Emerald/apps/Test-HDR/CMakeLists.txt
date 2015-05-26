# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (TestHDR) 

SET(CMAKE_INCLUDE_PATH "${TestHDR_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(TestHDRSourcesPrecompiledBinary "${TestHDR_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(TestHDRSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${TestHDRSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{TestHDRSourcesPrecompiledBinary}\" /Fp\"${TestHDRSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${TestHDRSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${TestHDRSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${TestHDRSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ENDIF(MSVC)

INCLUDE_DIRECTORIES("${TestHDR_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${TestHDR_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${TestHDR_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${TestHDR_SOURCE_DIR}/src/*.cc")

FILE(COPY "${TestHDR_SOURCE_DIR}/nave_o366.hdr" DESTINATION "${TestHDR_BINARY_DIR}/")

SET(AllTestHDRSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTestHDRSources)

ADD_EXECUTABLE(TestHDR WIN32 ${AllTestHDRSources} ${GeneralIncludes})
SET_TARGET_PROPERTIES(TestHDR PROPERTIES LINK_FLAGS "/MANIFEST:NO")

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(TestHDR EMERALD)
ADD_DEPENDENCIES(TestHDR EMERALD)