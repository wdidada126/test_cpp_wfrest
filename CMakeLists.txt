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

# ---- Database drivers (docs/01_libraries.md) ----
# Business targets link the interface target below, never raw OS library names.
# Stage one: well-known system locations, so stray CMAKE_PREFIX_PATH entries
# (e.g. a broken local mysql copy) cannot shadow the packaged library.
find_library(SQLITE3_LIBRARY NAMES sqlite3
    PATHS /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu /usr/local/lib
          /opt/homebrew/lib
    NO_DEFAULT_PATH)
find_path(SQLITE3_INCLUDE_DIR NAMES sqlite3.h
    PATHS /usr/include /usr/local/include /opt/homebrew/include
    NO_DEFAULT_PATH)
find_library(MYSQLCLIENT_LIBRARY NAMES mysqlclient mariadb
    PATHS /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu /usr/local/lib
          /opt/homebrew/lib
    NO_DEFAULT_PATH)
find_path(MYSQLCLIENT_INCLUDE_DIR NAMES mysql.h
    PATHS /usr/include /usr/local/include /opt/homebrew/include
    PATH_SUFFIXES mysql mariadb
    NO_DEFAULT_PATH)

# Stage two: whatever the environment offers (custom prefixes, vcpkg, ...).
if(NOT SQLITE3_LIBRARY)
    find_library(SQLITE3_LIBRARY NAMES sqlite3)
endif()
if(NOT SQLITE3_INCLUDE_DIR)
    find_path(SQLITE3_INCLUDE_DIR NAMES sqlite3.h)
endif()
if(NOT MYSQLCLIENT_LIBRARY)
    find_library(MYSQLCLIENT_LIBRARY NAMES mysqlclient mariadb)
endif()
if(NOT MYSQLCLIENT_INCLUDE_DIR)
    find_path(MYSQLCLIENT_INCLUDE_DIR NAMES mysql.h PATH_SUFFIXES mysql mariadb)
endif()

if(NOT SQLITE3_LIBRARY OR NOT SQLITE3_INCLUDE_DIR)
    message(FATAL_ERROR "sqlite3 development files not found (install libsqlite3-dev)")
endif()
if(NOT MYSQLCLIENT_LIBRARY OR NOT MYSQLCLIENT_INCLUDE_DIR)
    message(FATAL_ERROR "MySQL client development files not found (install libmysqlclient-dev)")
endif()

add_library(ecshop_db_deps INTERFACE)
target_include_directories(ecshop_db_deps INTERFACE
    ${SQLITE3_INCLUDE_DIR}
    ${MYSQLCLIENT_INCLUDE_DIR}
)
target_link_libraries(ecshop_db_deps INTERFACE
    ${SQLITE3_LIBRARY}
    ${MYSQLCLIENT_LIBRARY}
)

# ---- SQLite schema embedded at configure time (single source of truth) ----
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/docs/sql/sqlite/001_ecshop_catalog.sql"
     ECSHOP_SQLITE_SCHEMA_SQL)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/ecshop")
configure_file(
    src/infrastructure/db/SqliteSchema.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ecshop/SqliteSchema.h"
    @ONLY
)

# ---- Executable ----
set(ECSHOP_SOURCES
    src/main.cpp
    src/app/AppConfig.cpp
    src/shared/Money.cpp
    src/shared/TimeUtil.cpp
    src/infrastructure/db/SqliteDb.cpp
    src/infrastructure/db/MysqlDb.cpp
    src/infrastructure/db/DbFactory.cpp
    src/http/ApiError.cpp
    src/http/HttpUtil.cpp
    src/http/HealthRoutes.cpp
    src/http/CatalogRoutes.cpp
    src/http/ArticleRoutes.cpp
    src/infrastructure/SqlCatalogRepository.cpp
    src/infrastructure/SqlArticleRepository.cpp
)

add_executable(wfrest_server ${ECSHOP_SOURCES})

target_include_directories(wfrest_server PRIVATE
    include
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${WORKFLOW_INCLUDE_DIR}
)

# Link wfrest + workflow + pthread + db drivers
target_link_libraries(wfrest_server PRIVATE
    wfrest
    ${WORKFLOW_LIB_DIR}/libworkflow.a
    ecshop_db_deps
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
