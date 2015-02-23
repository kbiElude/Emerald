# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (Test-DemoApp) 

SET(CMAKE_INCLUDE_PATH "${Test-DemoApp_BINARY_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(Test-DemoAppSourcesPrecompiledBinary "${Test-DemoApp_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(Test-DemoAppSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${Test-DemoAppSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{Test-DemoAppSourcesPrecompiledBinary}\" /Fp\"${Test-DemoAppSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${Test-DemoAppSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${Test-DemoAppSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${Test-DemoAppSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
INCLUDE_DIRECTORIES("${Test-DemoApp_BINARY_DIR}")
INCLUDE_DIRECTORIES("${Test-DemoApp_BINARY_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/include")

FILE(GLOB GeneralIncludes "${Test-DemoApp_BINARY_DIR}/include/*.h")
FILE(GLOB GeneralSources "${Test-DemoApp_BINARY_DIR}/src/*.cc")

SET(AllTest-DemoAppSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTest-DemoAppSources)

ADD_EXECUTABLE(Test-DemoApp WIN32 ${AllTest-DemoAppSources} ${GeneralIncludes})
SET_TARGET_PROPERTIES(Test-DemoApp PROPERTIES LINK_FLAGS "/MANIFEST:NO")

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(Test-DemoApp EMERALD)
ADD_DEPENDENCIES(Test-DemoApp EMERALD)