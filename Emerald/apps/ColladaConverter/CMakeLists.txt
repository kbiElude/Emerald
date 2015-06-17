# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (ColladaConverter) 

SET(CMAKE_INCLUDE_PATH "${ColladaConverter_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(ColladaConverterSourcesPrecompiledBinary "${ColladaConverter_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(ColladaConverterSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${ColladaConverterSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{ColladaConverterSourcesPrecompiledBinary}\" /Fp\"${ColladaConverterSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${ColladaConverterSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${ColladaConverterSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${ColladaConverterSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ENDIF(MSVC)

INCLUDE_DIRECTORIES("${ColladaConverter_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${ColladaConverter_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${ColladaConverter_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${ColladaConverter_SOURCE_DIR}/src/*.cc")

SET(AllColladaConverterSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllColladaConverterSources)

ADD_EXECUTABLE(ColladaConverter WIN32 ${AllColladaConverterSources} ${GeneralIncludes})

IF (MSVC)
    SET_TARGET_PROPERTIES(ColladaConverter PROPERTIES LINK_FLAGS "/MANIFEST:NO")
ENDIF (MSVC)

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(ColladaConverter EMERALD)
ADD_DEPENDENCIES(ColladaConverter EMERALD)