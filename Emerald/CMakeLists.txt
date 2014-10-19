# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.8) 
project (EMERALD) 

# Options
OPTION(INCLUDE_CURVE_EDITOR    "Should include curve editor code" OFF)
OPTION(INCLUDE_OBJECT_MANAGER  "Should include object manager code (incl. resource leaking detection)" ON)
OPTION(INCLUDE_OPENCL          "Should include OpenCL code" OFF)
OPTION(INCLUDE_SH_LIGHT_EDITOR "Should include SH light editor code" OFF)
OPTION(INCLUDE_WEBCAM_MANAGER  "Should include webcam manager code" OFF)


# DO NOT MODIFY BELOW THIS LINE
include (GenerateExportHeader)

CONFIGURE_FILE ("${EMERALD_SOURCE_DIR}/include/config.h.in" "${EMERALD_SOURCE_DIR}/include/config.h")

SET(CMAKE_INCLUDE_PATH "${EMERALD_BINARY_DIR}/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${EMERALD_BINARY_DIR}/bin")

MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(PrecompiledBinary "${EMERALD_BINARY_DIR}/bin/${PrecompiledBasename}.pch")
    SET(Sources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${Sources}
                                PROPERTIES COMPILE_FLAGS "/W3 /analyze /Yu\"${PrecompiledHeader}\" /Fp\"{PrecompiledBinary}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_DEPENDS "${PrecompiledBinary}")  
    SET_SOURCE_FILES_PROPERTIES(${PrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/analyze /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
                                           OBJECT_OUTPUTS "${PrecompiledBinary}")
  ENDIF(MSVC)
  LIST(APPEND ${SourcesVar} ${PrecompiledSource})
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/include")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/!deps/jpeg-6b")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/!deps/libpng-1.2.36")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/!deps/lwsdk")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/!deps/tinyxml2")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/!deps/zlib-1.2.3")
INCLUDE_DIRECTORIES("${EMERALD_BINARY_DIR}/modules/system/include")
INCLUDE_DIRECTORIES("gtest-1.6.0/include")
INCLUDE_DIRECTORIES("gtest-1.6.0/")

FILE(GLOB ColladaIncludes "${EMERALD_BINARY_DIR}/include/collada/*.h")
FILE(GLOB ColladaSources "${EMERALD_BINARY_DIR}/src/collada/*.cc")
FILE(GLOB CurveIncludes "${EMERALD_BINARY_DIR}/include/curve/*.h")
FILE(GLOB CurveSources "${EMERALD_BINARY_DIR}/src/curve/*.cc")
FILE(GLOB GeneralIncludes "${EMERALD_BINARY_DIR}/include/*.h")
FILE(GLOB GeneralSources "${EMERALD_BINARY_DIR}/src/*.cc")
FILE(GLOB GFXIncludes "${EMERALD_BINARY_DIR}/include/gfx/*.h")
FILE(GLOB GFXSources "${EMERALD_BINARY_DIR}/src/gfx/*.cc")
FILE(GLOB LWIncludes "${EMERALD_BINARY_DIR}/include/lw/*.h")
FILE(GLOB LWSources "${EMERALD_BINARY_DIR}/src/lw/*.cc")
FILE(GLOB MeshIncludes "${EMERALD_BINARY_DIR}/include/mesh/*.h")
FILE(GLOB MeshSources "${EMERALD_BINARY_DIR}/src/mesh/*.cc")
FILE(GLOB OpenGLIncludes "${EMERALD_BINARY_DIR}/include/ogl/*.h")
FILE(GLOB OpenGLSources "${EMERALD_BINARY_DIR}/src/ogl/*.cc")
FILE(GLOB PostProcessingIncludes "${EMERALD_BINARY_DIR}/include/postprocessing/*.h")
FILE(GLOB PostProcessingSources "${EMERALD_BINARY_DIR}/src/postprocessing/*.cc")
FILE(GLOB ProceduralIncludes "${EMERALD_BINARY_DIR}/include/procedural/*.h")
FILE(GLOB ProceduralSources "${EMERALD_BINARY_DIR}/src/procedural/*.cc")
FILE(GLOB SceneIncludes "${EMERALD_BINARY_DIR}/include/scene/*.h")
FILE(GLOB SceneSources "${EMERALD_BINARY_DIR}/src/scene/*.cc")
FILE(GLOB ShIncludes "${EMERALD_BINARY_DIR}/include/sh/*.h")
FILE(GLOB ShSources "${EMERALD_BINARY_DIR}/src/sh/*.cc")
FILE(GLOB ShadersIncludes "${EMERALD_BINARY_DIR}/include/shaders/*.h")
FILE(GLOB ShadersSources "${EMERALD_BINARY_DIR}/src/shaders/*.cc")
FILE(GLOB SystemIncludes "${EMERALD_BINARY_DIR}/include/system/*.h")
FILE(GLOB SystemSources "${EMERALD_BINARY_DIR}/src/system/*.cc")
FILE(GLOB TextureIncludes "${EMERALD_BINARY_DIR}/include/texture/*.h")
FILE(GLOB TextureSources "${EMERALD_BINARY_DIR}/src/texture/*.cc")

IF (INCLUDE_WEBCAM_MANAGER)
    FILE(GLOB WebcamIncludes "${EMERALD_BINARY_DIR}/include/webcam/*.h")
    FILE(GLOB WebcamSources "${EMERALD_BINARY_DIR}/src/webcam/*.cc")

    FIND_LIBRARY(STRMIIDS_LIB strmiids)
ENDIF (INCLUDE_WEBCAM_MANAGER)

IF (INCLUDE_OBJECT_MANAGER)
    FILE(GLOB ObjectManagerIncludes "${EMERALD_BINARY_DIR}/include/object_manager/*.h")
    FILE(GLOB ObjectManagerSources  "${EMERALD_BINARY_DIR}/src/object_manager/*.cc")
ENDIF (INCLUDE_OBJECT_MANAGER)

IF (INCLUDE_CURVE_EDITOR)
    FILE(GLOB CurveEditorIncludes "${EMERALD_BINARY_DIR}/include/curve_editor/*.h")
    FILE(GLOB CurveEditorSources  "${EMERALD_BINARY_DIR}/src/curve_editor/*.cc")
ENDIF (INCLUDE_CURVE_EDITOR)

IF (INCLUDE_OPENCL)
    FILE(GLOB OpenCLIncludes "${EMERALD_BINARY_DIR}/include/ocl/*.h")
    FILE(GLOB OpenCLSources "${EMERALD_BINARY_DIR}/src/ocl/*.cc")
ENDIF (INCLUDE_OPENCL)

IF (INCLUDE_SH_LIGHT_EDITOR)
    FILE(GLOB ShLightEditorIncludes "${EMERALD_BINARY_DIR}/include/sh_light_editor/*.h")
    FILE(GLOB ShLightEditorSources "${EMERALD_BINARY_DIR}/src/sh_light_editor/*.cc")
ENDIF (INCLUDE_SH_LIGHT_EDITOR)

SET(AllSources ${GeneralSources}
               ${ColladaSources}
               ${SystemSources} 
               ${CurveSources} 
               ${LWSources}
               ${GFXSources}
               ${OpenGLSources} 
               ${WebcamSources} 
               ${ShadersSources} 
               ${ObjectManagerSources} 
               ${CurveEditorSources} 
               ${PostProcessingSources} 
               ${ShSources} 
               ${ShLightEditorSources}
               ${ProceduralSources}
               ${SceneSources} 
               ${MeshSources}
               ${OpenCLSources}
               ${TextureSources})
ADD_MSVC_PRECOMPILED_HEADER("shared.h" "src/shared.cc" AllSources)

ADD_LIBRARY(EMERALD SHARED ${AllSources}
    ${ColladaIncludes}
    ${CurveIncludes} 
    ${GeneralIncludes}
    ${LWIncludes}
    ${GFXIncludes} 
    ${SystemIncludes} 
    ${OpenGLIncludes} 
    ${WebcamIncludes} 
    ${ShadersIncludes} 
    ${ObjectManagerIncludes} 
    ${CurveEditorIncludes} 
    ${PostProcessingIncludes} 
    ${ShIncludes}
    ${ShLightEditorIncludes}
    ${ProceduralIncludes} 
    ${SceneIncludes} 
    ${MeshIncludes} 
    ${OpenCLIncludes} 
    ${TextureIncludes}
     Emerald.rc)

GENERATE_EXPORT_HEADER( EMERALD
	BASE_NAME EMERALD
	EXPORT_MACRO_NAME _EMERALD_API
	EXPORT_FILE_NAME ./include/generated_dll_exports.h
)

SET(LIBRARY_OUTPUT_PATH ${EMERALD_BINARY_DIR}/bin)

SOURCE_GROUP ("Collada includes" FILES ${ColladaIncludes})
SOURCE_GROUP ("Collada sources" FILES ${ColladaSources})
SOURCE_GROUP ("Curve Editor includes" FILES ${CurveEditorIncludes})
SOURCE_GROUP ("Curve Editor sources" FILES ${CurveEditorSources})
SOURCE_GROUP ("Curve includes" FILES ${CurveIncludes})
SOURCE_GROUP ("Curve sources" FILES ${CurveSources})
SOURCE_GROUP ("General includes" FILES ${GeneralIncludes})
SOURCE_GROUP ("General sources" FILES ${GeneralSources})
SOURCE_GROUP ("GFX includes" FILES ${GFXIncludes})
SOURCE_GROUP ("GFX sources" FILES ${GFXSources})
SOURCE_GROUP ("Lightwave includes" FILES ${LWIncludes})
SOURCE_GROUP ("Lightwave sources" FILES ${LWSources})
SOURCE_GROUP ("Mesh includes" FILES ${MeshIncludes})
SOURCE_GROUP ("Mesh sources" FILES ${MeshSources})
SOURCE_GROUP ("Object Manager includes" FILES ${ObjectManagerIncludes})
SOURCE_GROUP ("Object Manager sources" FILES ${ObjectManagerSources})
SOURCE_GROUP ("OpenCL includes" FILES ${OpenCLIncludes})
SOURCE_GROUP ("OpenCL sources" FILES ${OpenCLSources})
SOURCE_GROUP ("OpenGL includes" FILES ${OpenGLIncludes})
SOURCE_GROUP ("OpenGL sources" FILES ${OpenGLSources})
SOURCE_GROUP ("Post-processing includes" FILES ${PostProcessingIncludes})
SOURCE_GROUP ("Post-processing sources" FILES ${PostProcessingSources})
SOURCE_GROUP ("Procedural includes" FILES ${ProceduralIncludes})
SOURCE_GROUP ("Procedural sources" FILES ${ProceduralSources})
SOURCE_GROUP ("Scene includes" FILES ${SceneIncludes})
SOURCE_GROUP ("Scene sources" FILES ${SceneSources})
SOURCE_GROUP ("Shaders includes" FILES ${ShadersIncludes})
SOURCE_GROUP ("Shaders sources" FILES ${ShadersSources})
SOURCE_GROUP ("SH includes" FILES ${ShIncludes})
SOURCE_GROUP ("SH sources" FILES ${ShSources})
SOURCE_GROUP ("SH Light Editor includes" FILES ${ShLightEditorIncludes})
SOURCE_GROUP ("SH Light Editor sources" FILES ${ShLightEditorSources})
SOURCE_GROUP ("System includes" FILES ${SystemIncludes})
SOURCE_GROUP ("System sources" FILES ${SystemSources})
SOURCE_GROUP ("Texture includes" FILES ${TextureIncludes})
SOURCE_GROUP ("Texture sources" FILES ${TextureSources})
SOURCE_GROUP ("Webcam includes" FILES ${WebcamIncludes})
SOURCE_GROUP ("Webcam sources" FILES ${WebcamSources})

FILE(GLOB TestSources "test/*.cc")
ADD_EXECUTABLE(TEST ${TestSources} "gtest-1.6.0/src/gtest-all.cc")

FIND_PACKAGE(OpenGL)

TARGET_LINK_LIBRARIES(EMERALD ${OPENGL_LIBRARIES} ${STRMIIDS_LIB} LIB_JPEG LIB_PNG LIB_ZLIB tinyxml2static)
TARGET_LINK_LIBRARIES(TEST EMERALD)
ADD_DEPENDENCIES(EMERALD LIB_PNG)
ADD_DEPENDENCIES(TEST EMERALD)

ADD_SUBDIRECTORY(!apps)
ADD_SUBDIRECTORY(!deps)
