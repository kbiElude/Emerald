cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
cmake_policy(VERSION 2.6)

project(tinyxml2)
include(GNUInstallDirs)
#enable_testing()

#CMAKE_BUILD_TOOL

################################
# set lib version here

set(GENERIC_LIB_VERSION "2.0.0")
set(GENERIC_LIB_SOVERSION "2")


################################
# Add common source 

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/.")

################################
# Add definitions

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)

################################
# Add targets
set(BUILD_STATIC_LIBS ON CACHE BOOL "Set to ON to build static libraries")
if(BUILD_STATIC_LIBS)
	add_library(tinyxml2static STATIC tinyxml2.cpp tinyxml2.h)
	set_target_properties(tinyxml2static PROPERTIES OUTPUT_NAME tinyxml2)
endif(BUILD_STATIC_LIBS)


configure_file(tinyxml2.pc.in tinyxml2.pc @ONLY)

