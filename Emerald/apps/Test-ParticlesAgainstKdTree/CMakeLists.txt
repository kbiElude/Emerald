# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (TestParticlesAgainstKdTree) 

SET(CMAKE_INCLUDE_PATH "${TestParticlesAgainstKdTree_BINARY_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(TestParticlesAgainstKdTreeSourcesPrecompiledBinary "${TestParticlesAgainstKdTree_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(TestParticlesAgainstKdTreeSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${TestParticlesAgainstKdTreeSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{TestParticlesAgainstKdTreeSourcesPrecompiledBinary}\" /Fp\"${TestParticlesAgainstKdTreeSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${TestParticlesAgainstKdTreeSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${TestParticlesAgainstKdTreeSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${TestParticlesAgainstKdTreeSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ENDIF(MSVC)

INCLUDE_DIRECTORIES("${TestParticlesAgainstKdTree_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${TestParticlesAgainstKdTree_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/include")

FILE(GLOB GeneralIncludes "${TestParticlesAgainstKdTree_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${TestParticlesAgainstKdTree_SOURCE_DIR}/src/*.cc")

SET(AllTestParticlesAgainstKdTreeSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTestParticlesAgainstKdTreeSources)

ADD_EXECUTABLE(TestParticlesAgainstKdTree WIN32 ${AllTestParticlesAgainstKdTreeSources} ${GeneralIncludes})

IF (MSVC)
    SET_TARGET_PROPERTIES(TestParticlesAgainstKdTree PROPERTIES LINK_FLAGS "/MANIFEST:NO")
ENDIF (MSVC)

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(TestParticlesAgainstKdTree EMERALD)
ADD_DEPENDENCIES(TestParticlesAgainstKdTree EMERALD)