cmake_minimum_required(VERSION 3.15)
project(test_cpp_wfrest
    VERSION 0.1.0
    LANGUAGES C CXX
)

# ---- C++ standard ----
# CACHE so -DCMAKE_CXX_STANDARD=20 from the CI matrix wins over the default.
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard (17 or 20)")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Build type ----
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- Find installed wfrest (installed to /usr/local via make install) ----
# wfrest's installed config file has a bug (doesn't include targets),
# so we find the targets file directly.
find_path(WFREST_CMAKE_DIR wfrest-targets.cmake
    PATHS /usr/local/lib/cmake/wfrest
          ${CMAKE_INSTALL_PREFIX}/lib/cmake/wfrest
)
if(NOT WFREST_CMAKE_DIR)
    message(FATAL_ERROR "wfrest-targets.cmake not found. Is wfrest installed?")
endif()
include("${WFREST_CMAKE_DIR}/wfrest-targets.cmake")

# ---- Workflow dependency (bundled in wfrest source tree, not installed) ----
# workflow headers and libs live inside the wfrest source directory.
set(WFREST_SOURCE_DIR "/home/wdidada/git/github/wfrest"
    CACHE PATH "Path to wfrest source tree")
set(WORKFLOW_INCLUDE_DIR "${WFREST_SOURCE_DIR}/workflow/_include")
set(WORKFLOW_LIB_DIR      "${WFREST_SOURCE_DIR}/workflow/_lib")

if(NOT EXISTS "${WORKFLOW_INCLUDE_DIR}"
   OR NOT EXISTS "${WORKFLOW_LIB_DIR}/libworkflow.a")
    message(FATAL_ERROR
        "workflow not found under ${WFREST_SOURCE_DIR}/workflow "
        "(expected _include/ and _lib/libworkflow.a).\n"
        "Clone https://github.com/wfrest/wfrest.git with submodules, run `make` in it, "
        "then pass -DWFREST_SOURCE_DIR=<wfrest source dir>.")
endif()

# ---- Executable ----
add_executable(wfrest_server
    src/main.cpp
)

target_include_directories(wfrest_server PRIVATE ${WORKFLOW_INCLUDE_DIR})

# Link wfrest + workflow + pthread
target_link_libraries(wfrest_server PRIVATE
    wfrest
    ${WORKFLOW_LIB_DIR}/libworkflow.a
    pthread
    dl
    ssl
    crypto
    z
)

# ---- Install ----
install(TARGETS wfrest_server
    RUNTIME DESTINATION bin
)
