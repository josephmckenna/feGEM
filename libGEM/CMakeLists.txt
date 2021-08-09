# CMakeLists.txt for basic analysis package. It creates a library with dictionary and a main program
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(libGEM)

# Version numbering
set (basicApp_VERSION_MAJOR 1)
set (basicApp_VERSION_MAJOR 0)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enforce out-of-source builds 
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" _insource)
if(_insource)
   message(FATAL_ERROR "Error: In-source build detected! Please create a build directory!")
endif(_insource) 

# Checks $CLICOLOR_FORCE variable to force compiler to emit coloured output
# The parameter is based on compiler ID as it's not standardised
if ($ENV{CLICOLOR_FORCE})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(Clang|AppleClang)$")
       add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()

# Directories
add_subdirectory(src)

# You need to tell CMake where to find the ROOT installation. This can be done in a number of ways:
#   - ROOT built with classic configure/make use the provided $ROOTSYS/etc/cmake/FindROOT.cmake
#   - ROOT built with CMake. Add in CMAKE_PREFIX_PATH the installation prefix for ROOT
if(EXISTS $ENV{ROOTSYS}/ROOTConfig.cmake)
    if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
        list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})
    endif()
endif()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "$ENV{ROOTSYS}/etc/cmake")

# Locate the ROOT package and defines a number of variables (e.g. ROOT_INCLUDE_DIRS)
find_package(ROOT REQUIRED)
# Define useful ROOT functions and macros (e.g. ROOT_GENERATE_DICTIONARY)
include(${ROOT_USE_FILE})

# Include directories
include_directories(include ${ROOT_INCLUDE_DIRS})

ROOT_GENERATE_DICTIONARY(G__GEM TStoreGEMEvent.h LINKDEF LibLinkDef.hh )

add_library(GEM SHARED src/TStoreGEMEvent.cxx  G__GEM.cxx )
target_include_directories(GEM PUBLIC include)

target_link_libraries(GEM ${ROOT_LIBRARIES} )

add_executable(test src/test.cxx)
target_include_directories(test PUBLIC include)
target_link_libraries(test PUBLIC GEM)

install(TARGETS GEM DESTINATION lib)
install(FILES ${PROJECT_BINARY_DIR}/libGEM_rdict.pcm DESTINATION lib)
install(FILES ${PROJECT_BINARY_DIR}/libGEM.rootmap DESTINATION lib)
file(GLOB HEADER_FILES include/*.h)
install(FILES ${HEADER_FILES} DESTINATION include)

