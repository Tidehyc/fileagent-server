cmake_minimum_required(VERSION 3.20)

project(FileAgentServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FILEAGENT_ENABLE_DROGON "Enable Drogon integration when the dependency is available" ON)

# ─── vcpkg ────────────────────────────────────────────
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

find_package(yaml-cpp CONFIG REQUIRED)

find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
find_library(MYSQL_CLIENT_LIBRARY NAMES mysqlclient)

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_CLIENT_LIBRARY)
    message(FATAL_ERROR "MySQL client headers or library not found")
endif()

if(FILEAGENT_ENABLE_DROGON)
    find_package(Drogon CONFIG QUIET)
    if(Drogon_FOUND)
        message(STATUS "Drogon found, enabling HTTP framework integration")
    else()
        message(WARNING "Drogon not found; building fallback bootstrap stub")
        set(FILEAGENT_ENABLE_DROGON OFF)
    endif()
endif()

# ─── Sources ──────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(fileagent-server ${SOURCES})

target_include_directories(fileagent-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MYSQL_INCLUDE_DIR}
)

if(FILEAGENT_ENABLE_DROGON)
    target_compile_definitions(fileagent-server PRIVATE FILEAGENT_ENABLE_DROGON=1)
    target_link_libraries(fileagent-server PRIVATE Drogon::Drogon)
endif()

target_link_libraries(fileagent-server PRIVATE yaml-cpp::yaml-cpp)
target_link_libraries(fileagent-server PRIVATE ${MYSQL_CLIENT_LIBRARY})
target_link_libraries(fileagent-server PRIVATE crypt)
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY NAMES hiredis)
if(NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIBRARY)
    message(WARNING "hiredis not found; Redis cache support disabled")
else()
    target_include_directories(fileagent-server PRIVATE ${HIREDIS_INCLUDE_DIR})
    target_link_libraries(fileagent-server PRIVATE ${HIREDIS_LIBRARY})
    message(STATUS "hiredis found, Redis cache enabled")
endif()

target_compile_options(fileagent-server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)