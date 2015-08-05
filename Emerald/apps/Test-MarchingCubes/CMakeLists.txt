# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (Test-MarchingCubesApp) 

SET(CMAKE_INCLUDE_PATH "${Test-MarchingCubesApp_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(Test-MarchingCubesAppSourcesPrecompiledBinary "${Test-MarchingCubesApp_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(Test-MarchingCubesAppSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${Test-MarchingCubesAppSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{Test-MarchingCubesAppSourcesPrecompiledBinary}\" /Fp\"${Test-MarchingCubesAppSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${Test-MarchingCubesAppSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${Test-MarchingCubesAppSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${Test-MarchingCubesAppSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ENDIF(MSVC)

INCLUDE_DIRECTORIES("${Test-MarchingCubesApp_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${Test-MarchingCubesApp_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${Test-MarchingCubesApp_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${Test-MarchingCubesApp_SOURCE_DIR}/src/*.cc")

SET(AllTest-MarchingCubesAppSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTest-MarchingCubesAppSources)

ADD_EXECUTABLE(Test-MarchingCubesApp WIN32 ${AllTest-MarchingCubesAppSources} ${GeneralIncludes})

IF (MSVC)
    SET_TARGET_PROPERTIES(Test-MarchingCubesApp PROPERTIES LINK_FLAGS "/MANIFEST:NO")
ELSE (MSVC)
    # Force a 32-bit build
    SET_TARGET_PROPERTIES(Test-MarchingCubesApp PROPERTIES COMPILE_FLAGS "-m32"
                                                           LINK_FLAGS    "-m32")
ENDIF (MSVC)

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(Test-MarchingCubesApp EMERALD)
ADD_DEPENDENCIES(Test-MarchingCubesApp EMERALD)