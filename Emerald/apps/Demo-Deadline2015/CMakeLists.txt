cmake_minimum_required (VERSION 2.6) 
project (Demo-Deadline2015) 

SET(CMAKE_INCLUDE_PATH "${Demo-Deadline2015_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(Demo-Deadline2015SourcesPrecompiledBinary "${Demo-Deadline2015_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(Demo-Deadline2015Sources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${Demo-Deadline2015SourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{Demo-Deadline2015SourcesPrecompiledBinary}\" /Fp\"${Demo-Deadline2015SourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${Demo-Deadline2015SourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${Demo-Deadline2015SourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${Demo-Deadline2015SourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
ENDIF(MSVC)

INCLUDE_DIRECTORIES("${Demo-Deadline2015_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${Demo-Deadline2015_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${Demo-Deadline2015_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${Demo-Deadline2015_SOURCE_DIR}/src/*.cc")

SET(AllDemo-Deadline2015Sources ${GeneralSources} )

ADD_EXECUTABLE(Demo-Deadline2015 WIN32 ${AllDemo-Deadline2015Sources} ${GeneralIncludes})

IF (MSVC)
    SET_TARGET_PROPERTIES(Demo-Deadline2015 PROPERTIES LINK_FLAGS "/MANIFEST:NO")
ELSE (MSVC)
    # Force a 32-bit build
    SET_TARGET_PROPERTIES(Demo-Deadline2015 PROPERTIES COMPILE_FLAGS "-m32"
                                                           LINK_FLAGS    "-m32")
ENDIF (MSVC)

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(Demo-Deadline2015 EMERALD)
ADD_DEPENDENCIES(Demo-Deadline2015 EMERALD)