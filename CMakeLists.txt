########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.0)
project(rx_tools C)
set(CMAKE_C_STANDARD 99)

#local include directories first
include_directories(${PROJECT_SOURCE_DIR}/src/convenience)

#include local cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

########################################################################
# Dependencies
########################################################################
find_package(SoapySDR "0.5" NO_MODULE)
if (NOT SoapySDR_FOUND)
    message(FATAL_ERROR "Soapy SDR development files not found...")
endif ()
include_directories(${SoapySDR_INCLUDE_DIRS})
list(APPEND RX_TOOLS_LIBS ${SoapySDR_LIBRARIES})

#link with libm when available
find_library(
    MATH_LIBRARIES NAMES m
    PATHS /usr/lib /usr/lib64
)
if (MATH_LIBRARIES)
    list(APPEND RX_TOOLS_LIBS ${MATH_LIBRARIES})
endif ()

#link with pthreads
set(THREADS_USE_PTHREADS_WIN32 true)
find_package(Threads)
if (NOT THREADS_FOUND)
    message(FATAL_ERROR "pthreads development files not found...")
endif ()
include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
list(APPEND RX_TOOLS_LIBS ${CMAKE_THREAD_LIBS_INIT})
message(STATUS "THREADS_PTHREADS_INCLUDE_DIR: ${THREADS_PTHREADS_INCLUDE_DIR}")
message(STATUS "CMAKE_THREAD_LIBS_INIT: ${CMAKE_THREAD_LIBS_INIT}")

#windows getopt compatibility
if (WIN32)
    include_directories(${PROJECT_SOURCE_DIR}/src/getopt)
    list(APPEND COMMON_SOURCES src/getopt/getopt.c)
endif ()

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    #disable warnings for unused parameters
    add_definitions(-Wno-unused-parameter)
endif(CMAKE_C_COMPILER_ID STREQUAL "GNU")

########################################################################
# Helper library
########################################################################
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
list(APPEND COMMON_SOURCES src/convenience/convenience.c)
add_library(common STATIC ${COMMON_SOURCES})
list(APPEND RX_TOOLS_LIBS common)

########################################################################
# Build executables
########################################################################
add_executable(rx_fm src/rtl_fm.c)
target_link_libraries(rx_fm ${RX_TOOLS_LIBS})

add_executable(rx_power src/rtl_power.c)
target_link_libraries(rx_power ${RX_TOOLS_LIBS})

add_executable(rx_sdr src/rtl_sdr.c)
target_link_libraries(rx_sdr ${RX_TOOLS_LIBS})

########################################################################
# Install executables
########################################################################
install(TARGETS rx_fm rx_power rx_sdr DESTINATION bin)
