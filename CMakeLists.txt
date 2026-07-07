cmake_minimum_required(VERSION 3.20)

project(FileAgentServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── vcpkg ────────────────────────────────────────────
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

find_package(yaml-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
find_library(MYSQL_CLIENT_LIBRARY NAMES mysqlclient)

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_CLIENT_LIBRARY)
    message(FATAL_ERROR "MySQL client headers or library not found")
endif()

find_library(FCGI_LIBRARY NAMES fcgi)
if(NOT FCGI_LIBRARY)
    message(FATAL_ERROR "libfcgi not found (apt install libfcgi-dev)")
endif()

# ─── Sources ──────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(fileagent-server ${SOURCES})

set(AGENTS_CPP_DIR $ENV{HOME}/workplace/agents.cpp-main)

find_path(FDFS_INCLUDE_DIR fastdfs/fdfs_client.h)
if(NOT FDFS_INCLUDE_DIR)
    message(WARNING "FastDFS headers not found; FastDFS storage disabled")
endif()

find_library(FDFS_LIBRARY NAMES fdfsclient)
if(NOT FDFS_LIBRARY)
    message(WARNING "FastDFS library not found; FastDFS storage disabled")
endif()

target_include_directories(fileagent-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MYSQL_INCLUDE_DIR}
    ${AGENTS_CPP_DIR}/include
    ${FDFS_INCLUDE_DIR}
    /usr/include/fastcommon
)

# Link agents-cpp precompiled library
if(EXISTS ${AGENTS_CPP_DIR}/lib/linux/x64/libagents_cpp_shared_lib.so)
    target_link_libraries(fileagent-server PRIVATE
        ${AGENTS_CPP_DIR}/lib/linux/x64/libagents_cpp_shared_lib.so)
    message(STATUS "Found agents-cpp shared library")
else()
    message(WARNING "agents-cpp shared library not found; LLM features disabled")
endif()

target_link_libraries(fileagent-server PRIVATE
    yaml-cpp::yaml-cpp
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
    ${MYSQL_CLIENT_LIBRARY}
    ${FCGI_LIBRARY}
    ${FDFS_LIBRARY}
    fastcommon
    serverframe
    crypt
)

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

# ─── 单元测试 ──────────────────────────────────────────
find_package(GTest CONFIG QUIET)
if(GTest_FOUND)
    enable_testing()

    # 需测试的公共源文件（不含 agents-cpp / MySQL / FastDFS 依赖）
    set(TEST_COMMON_SOURCES
        src/common/TextExtractor.cpp
        src/vector/VectorStore.cpp
        src/vector/TextChunker.cpp
    )

    add_executable(fileagent-test
        tests/test_main.cpp
        tests/test_text_extractor.cpp
        tests/test_vector_store.cpp
        tests/test_text_chunker.cpp
        ${TEST_COMMON_SOURCES}
    )

    target_include_directories(fileagent-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(fileagent-test PRIVATE
        GTest::gtest
        GTest::gtest_main
    )

    add_test(NAME FileAgentUnitTest COMMAND fileagent-test)
else()
    message(WARNING "GTest not found; unit tests disabled")
endif()
