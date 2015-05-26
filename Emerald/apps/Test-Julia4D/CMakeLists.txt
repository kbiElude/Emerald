# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (TestJulia4D) 

SET(CMAKE_INCLUDE_PATH "${TestJulia4D_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(TestJulia4DSourcesPrecompiledBinary "${TestJulia4D_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(TestJulia4DSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${TestJulia4DSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{TestJulia4DSourcesPrecompiledBinary}\" /Fp\"${TestJulia4DSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${TestJulia4DSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${TestJulia4DSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${TestJulia4DSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ENDIF(MSVC)

INCLUDE_DIRECTORIES("${TestJulia4D_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${TestJulia4D_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${TestJulia4D_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${TestJulia4D_SOURCE_DIR}/src/*.cc")

SET(AllTestJulia4DSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTestJulia4DSources)

ADD_EXECUTABLE(TestJulia4D WIN32 ${AllTestJulia4DSources} ${GeneralIncludes})
SET_TARGET_PROPERTIES(TestJulia4D PROPERTIES LINK_FLAGS "/MANIFEST:NO")

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(TestJulia4D EMERALD)
ADD_DEPENDENCIES(TestJulia4D EMERALD)