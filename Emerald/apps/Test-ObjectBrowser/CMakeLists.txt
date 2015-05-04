# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (TestObjectBrowser) 

SET(CMAKE_INCLUDE_PATH "${TestObjectBrowser_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(TestObjectBrowserSourcesPrecompiledBinary "${TestObjectBrowser_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(TestObjectBrowserSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${TestObjectBrowserSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{TestObjectBrowserSourcesPrecompiledBinary}\" /Fp\"${CamMuxerSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${TestObjectBrowserSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${TestObjectBrowserSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${TestObjectBrowserSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
INCLUDE_DIRECTORIES("${TestObjectBrowser_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${TestObjectBrowser_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${TestObjectBrowser_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${TestObjectBrowser_SOURCE_DIR}/src/*.cc")

SET(AllTestObjectBrowserSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTestObjectBrowserSources)

ADD_EXECUTABLE(TestObjectBrowser WIN32 ${AllTestObjectBrowserSources} ${GeneralIncludes})
SET_TARGET_PROPERTIES(TestObjectBrowser PROPERTIES LINK_FLAGS "/MANIFEST:NO")

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(TestObjectBrowser EMERALD)
ADD_DEPENDENCIES(TestObjectBrowser EMERALD)