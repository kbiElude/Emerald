# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 

ADD_SUBDIRECTORY(ColladaConverter)
ADD_SUBDIRECTORY(ES31AppTemplate)
ADD_SUBDIRECTORY(LW-AnimationDataExtractor)
ADD_SUBDIRECTORY(SceneViewer)
ADD_SUBDIRECTORY(TextureCompressor)
ADD_SUBDIRECTORY(Test-DemoApp)
ADD_SUBDIRECTORY(Test-DOF)
ADD_SUBDIRECTORY(Test-HDR)
ADD_SUBDIRECTORY(Test-Julia4D)
ADD_SUBDIRECTORY(Test-ObjectBrowser)
ADD_SUBDIRECTORY(Test-Particles)
ADD_SUBDIRECTORY(Test-ParticlesAgainstKdTree)
ADD_SUBDIRECTORY(Test-Pipeline)
ADD_SUBDIRECTORY(Test-Uber)