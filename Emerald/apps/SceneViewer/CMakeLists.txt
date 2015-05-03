# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (SceneViewer) 

SET(CMAKE_INCLUDE_PATH "${SceneViewer_BINARY_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(SceneViewerSourcesPrecompiledBinary "${SceneViewer_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(SceneViewerSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${SceneViewerSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{SceneViewerSourcesPrecompiledBinary}\" /Fp\"${SceneViewerSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${SceneViewerSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${SceneViewerSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${SceneViewerSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
INCLUDE_DIRECTORIES("${SceneViewer_BINARY_DIR}")
INCLUDE_DIRECTORIES("${SceneViewer_BINARY_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/include")

FILE(GLOB GeneralIncludes "${SceneViewer_BINARY_DIR}/include/*.h")
FILE(GLOB GeneralSources "${SceneViewer_BINARY_DIR}/src/*.cc")

SET(AllSceneViewerSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllSceneViewerSources)

ADD_EXECUTABLE(SceneViewer WIN32 ${AllSceneViewerSources} ${GeneralIncludes})
SET_TARGET_PROPERTIES(SceneViewer PROPERTIES LINK_FLAGS "/MANIFEST:NO")

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(SceneViewer EMERALD)
ADD_DEPENDENCIES(SceneViewer EMERALD)