# AM++ Library Build

set(AMPP_SOURCES
    mpi_make_mpi_datatype.cpp
    mpi_sinha_kale_ramkumar_termination_detector.cpp
    mpi_sinha_kale_ramkumar_termination_detector_bgp.cpp
    mpi_transport.cpp
    termination_detector.cpp
    thread_support.cpp
    transport.cpp
)

add_library(ampp ${AMPP_SOURCES})
add_library(ampp::ampp ALIAS ampp)

# Include directories - BUILD_INTERFACE order matters, put binary dir first
# to prefer CMake-generated config.h over autotools config.h
target_include_directories(ampp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Link dependencies
target_link_libraries(ampp
    PUBLIC
        MPI::MPI_CXX
        Threads::Threads
        Boost::system
        Boost::thread
)

# Threading and config compile definitions - PUBLIC so downstream consumers
# compile headers with the same settings the library was built with
if(AMPLUSPLUS_SINGLE_THREADED)
    target_compile_definitions(ampp PUBLIC AMPLUSPLUS_SINGLE_THREADED)
elseif(AMPLUSPLUS_USE_THREAD_SERIALIZED)
    target_compile_definitions(ampp PUBLIC AMPLUSPLUS_USE_THREAD_SERIALIZED)
elseif(AMPLUSPLUS_USE_THREAD_MULTIPLE)
    target_compile_definitions(ampp PUBLIC AMPLUSPLUS_USE_THREAD_MULTIPLE)
endif()

if(DISABLE_SELF_SEND_CHECK)
    target_compile_definitions(ampp PUBLIC DISABLE_SELF_SEND_CHECK)
endif()

# Set library version
set_target_properties(ampp PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME ampp
)

# Install library
install(TARGETS ampp
    EXPORT amppTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
