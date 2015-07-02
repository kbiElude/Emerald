# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6)

ADD_SUBDIRECTORY(ES31AppTemplate)

IF(MSVC)
    # Windows-only projects.
    #
    # These either use curve editor or object browser (both of which are
    # not supported under Linux)
    ADD_SUBDIRECTORY(ColladaConverter)
    ADD_SUBDIRECTORY(LW-AnimationDataExtractor)
    ADD_SUBDIRECTORY(SceneViewer)
    ADD_SUBDIRECTORY(Test-HDR)
    ADD_SUBDIRECTORY(Test-ObjectBrowser)
    ADD_SUBDIRECTORY(Test-Particles)
    ADD_SUBDIRECTORY(TextureCompressor)
ENDIF(MSVC)

ADD_SUBDIRECTORY(Test-ComputeShaderSSBO)
ADD_SUBDIRECTORY(Test-DemoApp)
ADD_SUBDIRECTORY(Test-DOF)
ADD_SUBDIRECTORY(Test-Julia4D)
ADD_SUBDIRECTORY(Test-Pipeline)


# Following are obsolete examples which no longer build or whose asset became
# incompatible with the recent Emerald versions. As such, they are no longer
# maintained.
#
# These examples are left as reference only.

# ADD_SUBDIRECTORY(Test-ParticlesAgainstKdTree)
# ADD_SUBDIRECTORY(Test-Uber)
