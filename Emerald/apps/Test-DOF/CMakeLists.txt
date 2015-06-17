# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (TestDOF) 

SET(CMAKE_INCLUDE_PATH "${TestDOF_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(TestDOFSourcesPrecompiledBinary "${TestDOF_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(TestDOFSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${TestDOFSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{TestDOFSourcesPrecompiledBinary}\" /Fp\"${TestDOFSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${TestDOFSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${TestDOFSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${TestDOFSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ENDIF(MSVC)

INCLUDE_DIRECTORIES("${TestDOF_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${TestDOF_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${TestDOF_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${TestDOF_SOURCE_DIR}/src/*.cc")

FILE(COPY "${TestDOF_SOURCE_DIR}/galileo_probe.hdr" DESTINATION "${TestDOF_BINARY_DIR}/")

SET(AllTestDOFSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTestDOFSources)

ADD_EXECUTABLE(TestDOF WIN32 ${AllTestDOFSources} ${GeneralIncludes})

IF (MSVC)
    SET_TARGET_PROPERTIES(TestDOF PROPERTIES LINK_FLAGS "/MANIFEST:NO")
ENDIF (MSVC)

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(TestDOF EMERALD)
ADD_DEPENDENCIES(TestDOF EMERALD)