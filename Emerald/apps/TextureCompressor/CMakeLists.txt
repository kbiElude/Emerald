# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (TextureCompressor) 

SET(CMAKE_INCLUDE_PATH "${TextureCompressor_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(TextureCompressorSourcesPrecompiledBinary "${TextureCompressor_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(TextureCompressorSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${TextureCompressorSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{TextureCompressorSourcesPrecompiledBinary}\" /Fp\"${TextureCompressorSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${TextureCompressorSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${TextureCompressorSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${TextureCompressorSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ENDIF(MSVC)

INCLUDE_DIRECTORIES("${TextureCompressor_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${TextureCompressor__DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${TextureCompressor_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${TextureCompressor_SOURCE_DIR}/src/*.cc")

SET(AllTextureCompressorSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTextureCompressorSources)

ADD_EXECUTABLE(TextureCompressor WIN32 ${AllTextureCompressorSources} ${GeneralIncludes})

IF (MSVC)
    SET_TARGET_PROPERTIES(TextureCompressor PROPERTIES LINK_FLAGS "/MANIFEST:NO")
ENDIF (MSVC)

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(TextureCompressor EMERALD)
ADD_DEPENDENCIES(TextureCompressor EMERALD)