# CMakeLists.txt for basic analysis package. It creates a library with dictionary and a main program
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(agdaq)

# Version numbering
set (basicApp_VERSION_MAJOR 1)
set (basicApp_VERSION_MAJOR 0)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
add_executable(feevb.exe feevb TsSync )
target_link_libraries(feevb.exe ${LIB} ${LIBS} )


add_executable(mlu_gen.exe mlu_gen)

add_executable(fexudp.exe fexudp )
target_link_libraries(fexudp.exe ${LIB} ${LIBS} )

add_executable(fecaenr14xxet.exe fecaenr14xxet KOtcp )
target_link_libraries(fecaenr14xxet.exe ${LIB} ${LIBS} )

add_executable(fewienerlvps.exe fewienerlvps )
target_link_libraries(fewienerlvps.exe ${LIB} ${LIBS} )

add_executable(femoxa.exe femoxa KOtcp )
target_link_libraries(femoxa.exe ${LIB} ${LIBS} )

add_executable(fegastelnet.exe fegastelnet KOtcp )
target_link_libraries(fegastelnet.exe ${LIB} ${LIBS} )

add_executable(fectrl.exe fectrl KOtcp )
target_link_libraries(fectrl.exe ${LIB} ${LIBS} )

add_executable(febvlvdb.exe febvlvdb KOtcp JsonTo EsperComm febvlvdb )
target_link_libraries(febvlvdb.exe ${LIB} ${LIBS} )


add_executable(test_tcp.exe KOtcp)
target_compile_definitions(test_tcp.exe PRIVATE MAIN)
target_link_libraries(test_tcp.exe ${LIB} ${LIBS} )

#add_executable(feLabVIEW.exe feLabVIEW)
#target_link_libraries(feLabVIEW.exe ${LIB} ${LIBS} zmq )

foreach( SEQID A B C D E F G )
   #string(CONCAT BIN [fe $(SEQID) sequencer] )
   set( BIN fe${SEQID}sequencer )
   add_executable(${BIN}.exe feXsequencer)
   target_compile_definitions(${BIN}.exe PRIVATE SEQID=\"${SEQID}\")
   target_link_libraries(${BIN}.exe ${LIB} ${LIBS} )
endforeach( SEQID A B C D E F G )

#A little clunky:
install(TARGETS 
feevb.exe
mlu_gen.exe
fexudp.exe
fecaenr14xxet.exe
fewienerlvps.exe
femoxa.exe
fegastelnet.exe
fectrl.exe
febvlvdb.exe
test_tcp.exe
 DESTINATION "${CMAKE_INSTALL_PREFIX}/frontends")
