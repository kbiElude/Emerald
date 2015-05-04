# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (TestUber) 

SET(CMAKE_INCLUDE_PATH "${TestUber_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(TestUberSourcesPrecompiledBinary "${TestUber_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(TestUberSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${TestUberSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{TestUberSourcesPrecompiledBinary}\" /Fp\"${TestUberSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${TestUberSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${TestUberSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${TestUberSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
INCLUDE_DIRECTORIES("${TestUber_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${TestUber_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${TestUber_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${TestUber_SOURCE_DIR}/src/*.cc")

SET(AllTestUberSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllTestUberSources)

ADD_EXECUTABLE(TestUber WIN32 ${AllTestUberSources} ${GeneralIncludes})
SET_TARGET_PROPERTIES(TestUber PROPERTIES LINK_FLAGS "/MANIFEST:NO")

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(TestUber EMERALD)
ADD_DEPENDENCIES(TestUber EMERALD)