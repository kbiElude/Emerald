# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 
project (ES31AppTemplate) 

SET(CMAKE_INCLUDE_PATH "${ES31AppTemplate_SOURCE_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(ES31AppTemplateSourcesPrecompiledBinary "${ES31AppTemplate_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(ES31AppTemplateSources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${ES31AppTemplateSourcesSources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{ES31AppTemplateSourcesPrecompiledBinary}\" /Fp\"${ES31AppTemplateSourcesPrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${ES31AppTemplateSourcesPrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${ES31AppTemplateSourcesPrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${ES31AppTemplateSourcesPrecompiledBinary}")

    # Add precompiled header to SourcesVar
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
INCLUDE_DIRECTORIES("${ES31AppTemplate_SOURCE_DIR}")
INCLUDE_DIRECTORIES("${ES31AppTemplate_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_SOURCE_DIR}/include")

FILE(GLOB GeneralIncludes "${ES31AppTemplate_SOURCE_DIR}/include/*.h")
FILE(GLOB GeneralSources "${ES31AppTemplate_SOURCE_DIR}/src/*.cc")

SET(AllES31AppTemplateSources ${GeneralSources} )

ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllES31AppTemplateSources)

ADD_EXECUTABLE(ES31AppTemplate WIN32 ${AllES31AppTemplateSources} ${GeneralIncludes})
SET_TARGET_PROPERTIES(ES31AppTemplate PROPERTIES LINK_FLAGS "/MANIFEST:NO")

SET(EXECUTABLE_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})

TARGET_LINK_LIBRARIES(ES31AppTemplate EMERALD)
ADD_DEPENDENCIES(ES31AppTemplate EMERALD)