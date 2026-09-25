cmake_minimum_required(VERSION 3.15)
project(test_cpp_wfrest
    VERSION 0.1.0
    LANGUAGES C CXX
)

# ---- C++ standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Build type ----
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- Options ----
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# ---- FetchContent: pull in wfrest (and its dependency workflow) ----
include(FetchContent)

FetchContent_Declare(
    wfrest
    GIT_REPOSITORY https://github.com/WFerns/wfrest.git
    GIT_TAG        main
)

# wfrest builds workflow internally via its own CMake,
# so we only need to fetch wfrest.
FetchContent_MakeAvailable(wfrest)

# ---- Executable ----
add_executable(wfrest_server
    src/main.cpp
)

# Link against wfrest's CMake target (also brings in workflow transitively)
target_link_libraries(wfrest_server PRIVATE wfrest)

# ---- Install ----
install(TARGETS wfrest_server
    RUNTIME DESTINATION bin
)
