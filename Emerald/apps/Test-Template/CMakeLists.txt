# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (TestPipeline) 

SET(CMAKE_INCLUDE_PATH "${TestPipeline_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(TestPipelineSourcesPrecompiledBinary "${TestPipeline_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(TestPipelineSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${TestPipelineSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{TestPipelineSourcesPrecompiledBinary}\" /Fp\"${TestPipelineSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${TestPipelineSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${TestPipelineSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${TestPipelineSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
INCLUDE_DIRECTORIES("${TestPipeline_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${TestPipeline_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${TestPipeline_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${TestPipeline_SOURCE_DIR}/src/*.cc")

SET(AllTestPipelineSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTestPipelineSources)

ADD_EXECUTABLE(TestPipeline WIN32 ${AllTestPipelineSources} ${GeneralIncludes})
SET_TARGET_PROPERTIES(TestPipeline PROPERTIES LINK_FLAGS "/MANIFEST:NO")

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(TestPipeline EMERALD)
ADD_DEPENDENCIES(TestPipeline EMERALD)