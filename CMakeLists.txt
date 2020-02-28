# CMakeLists.txt for basic analysis package. It creates a library with dictionary and a main program
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(agdaq)

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
#Extra path for gitinfo.h file
include_directories(${CMAKE_CURRENT_BINARY_DIR})



# You need to tell CMake where to find the ROOT installation. This can be done in a number of ways:
#   - ROOT built with classic configure/make use the provided $ROOTSYS/etc/cmake/FindROOT.cmake
#   - ROOT built with CMake. Add in CMAKE_PREFIX_PATH the installation prefix for ROOT
if(EXISTS $ENV{ROOTSYS}/ROOTConfig.cmake)
    if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
        list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})
    endif()
endif()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "$ENV{ROOTSYS}/etc/cmake")


# Directories
#add_subdirectory($ENV{MIDASSYS}/src)
add_compile_definitions(OS_LINUX extname)
#link_libraries(agana AGTPC aged)
link_directories($ENV{MIDASSYS}/lib)
link_directories($ENV{MIDASSYS}/mxml)

# Include directories
include_directories(${CMAKE_SOURCE_DIR})
include_directories($ENV{MIDASSYS}/include)
include_directories($ENV{MIDASSYS}/mxml)

set(LIB midas rt mfe)
set(LIBS  m z util nsl pthread)

#add_executable(mlu_gen)
add_executable(feevb feevb TsSync tmodb tmfe)
target_link_libraries(feevb ${LIB} ${LIBS} )


add_executable(mlu_gen mlu_gen)

add_executable(fexudp fexudp tmodb tmfe)
target_link_libraries(fexudp ${LIB} ${LIBS} )

add_executable(fecaenr14xxet fecaenr14xxet KOtcp tmfe tmodb)
target_link_libraries(fecaenr14xxet ${LIB} ${LIBS} )

add_executable(fewienerlvps fewienerlvps tmfe tmodb)
target_link_libraries(fewienerlvps ${LIB} ${LIBS} )

add_executable(femoxa femoxa KOtcp tmfe tmodb)
target_link_libraries(femoxa ${LIB} ${LIBS} )

add_executable(fegastelnet fegastelnet KOtcp tmfe tmodb)
target_link_libraries(fegastelnet ${LIB} ${LIBS} )

add_executable(fectrl fectrl KOtcp tmfe tmodb)
target_link_libraries(fectrl ${LIB} ${LIBS} )

add_executable(febvlvdb febvlvdb KOtcp JsonTo EsperComm febvlvdb tmfe tmodb)
target_link_libraries(febvlvdb ${LIB} ${LIBS} )


add_executable(test_tcp KOtcp)
target_compile_definitions(test_tcp PRIVATE MAIN)
target_link_libraries(test_tcp ${LIB} ${LIBS} )

