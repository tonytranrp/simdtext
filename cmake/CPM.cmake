# CPM.cmake - CMake Package Manager integration for simdtext
# Usage:
#   include(cmake/CPM.cmake)
#   CPMAddPackage("gh:tonytranrp/simdtext#v0.1.0")
#   target_link_libraries(my_target simdtext::simdtext)

if(NOT COMMAND CPMAddPackage)
    # Download CPM.cmake if not already available
    set(CPM_DOWNLOAD_VERSION 0.40.2)
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/_deps/CPM.cmake")

    if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
        message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}")
        file(DOWNLOAD
            "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
            "${CPM_DOWNLOAD_LOCATION}"
        )
    endif()
    include("${CPM_DOWNLOAD_LOCATION}")
endif()
