# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (TestComputeShaderSSBO) 

SET(CMAKE_INCLUDE_PATH "${TestComputeShaderSSBO_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(TestComputeShaderSSBOSourcesPrecompiledBinary "${TestComputeShaderSSBO_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(TestComputeShaderSSBOSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${TestComputeShaderSSBOSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{TestComputeShaderSSBOSourcesPrecompiledBinary}\" /Fp\"${TestComputeShaderSSBOSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${TestComputeShaderSSBOSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${TestComputeShaderSSBOSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${TestComputeShaderSSBOSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ENDIF(MSVC)

INCLUDE_DIRECTORIES("${TestComputeShaderSSBO_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${TestComputeShaderSSBO_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${TestComputeShaderSSBO_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${TestComputeShaderSSBO_SOURCE_DIR}/src/*.cc")

SET(AllTestComputeShaderSSBOSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTestComputeShaderSSBOSources)

ADD_EXECUTABLE(TestComputeShaderSSBO WIN32 ${AllTestComputeShaderSSBOSources} ${GeneralIncludes})

IF (MSVC)
    SET_TARGET_PROPERTIES(TestComputeShaderSSBO PROPERTIES LINK_FLAGS "/MANIFEST:NO")
ELSE (MSVC)
    # Force a 32-bit build
    SET_TARGET_PROPERTIES(TestComputeShaderSSBO PROPERTIES COMPILE_FLAGS "-m32"
                                                           LINK_FLAGS    "-m32")
ENDIF (MSVC)

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(TestComputeShaderSSBO EMERALD)
ADD_DEPENDENCIES(TestComputeShaderSSBO EMERALD)