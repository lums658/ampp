# Copyright 2013 The Trustees of Indiana University
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Jeremiah Willcock
#          Andrew Lumsdaine
#          Marcin Zalewski

cmake_minimum_required(VERSION 3.16)

project(ampp
    VERSION 0.999
    DESCRIPTION "AM++: High-performance asynchronous active-messages runtime"
    HOMEPAGE_URL "http://crest.iu.edu/projects/am++"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(AMPP_ENABLE_THREADING "Enable threading support" ON)
set(AMPP_THREADING_MODEL "serialized" CACHE STRING "Threading model: serialized, multiple, or none")
set_property(CACHE AMPP_THREADING_MODEL PROPERTY STRINGS "serialized" "multiple" "none")

option(AMPP_ENABLE_SELFCHECK "Enable self-checking (disable self-send check)" OFF)
option(AMPP_ENABLE_DEBUGGING "Enable debugging (disables NDEBUG)" OFF)
option(BUILD_TESTING "Build test programs" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Find required dependencies
find_package(MPI REQUIRED)
find_package(Threads REQUIRED)

# Find Boost - for now we still need some Boost libraries
# (Graph, Intrusive, Pool have no std replacements)
find_package(Boost 1.52 REQUIRED COMPONENTS system thread)

# Set threading configuration variables for config.h generation
if(NOT AMPP_ENABLE_THREADING OR AMPP_THREADING_MODEL STREQUAL "none")
    set(AMPLUSPLUS_SINGLE_THREADED 1)
elseif(AMPP_THREADING_MODEL STREQUAL "serialized")
    set(AMPLUSPLUS_USE_THREAD_SERIALIZED 1)
elseif(AMPP_THREADING_MODEL STREQUAL "multiple")
    set(AMPLUSPLUS_USE_THREAD_MULTIPLE 1)
endif()

# Set self-check configuration
if(AMPP_ENABLE_SELFCHECK)
    set(DISABLE_SELF_SEND_CHECK 1)
endif()

# Generate config header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    @ONLY
)

# Add subdirectories
add_subdirectory(src)

# Testing
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install headers
install(
    DIRECTORY am++/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/am++
    FILES_MATCHING PATTERN "*.hpp"
)

# Install config header
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Generate and install package config files
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/amppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/amppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/amppConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ampp
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/amppConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/amppConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ampp
)

install(
    EXPORT amppTargets
    FILE amppTargets.cmake
    NAMESPACE ampp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ampp
)

# Generate pkg-config file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libampp.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libampp.pc"
    @ONLY
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/libampp.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Print configuration summary
message(STATUS "")
message(STATUS "AM++ Configuration Summary")
message(STATUS "==========================")
message(STATUS "Version:          ${PROJECT_VERSION}")
message(STATUS "C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "Threading:        ${AMPP_ENABLE_THREADING}")
message(STATUS "Threading Model:  ${AMPP_THREADING_MODEL}")
message(STATUS "Self-check:       ${AMPP_ENABLE_SELFCHECK}")
message(STATUS "Debugging:        ${AMPP_ENABLE_DEBUGGING}")
message(STATUS "Build Tests:      ${BUILD_TESTING}")
message(STATUS "Shared Libs:      ${BUILD_SHARED_LIBS}")
message(STATUS "Install Prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
