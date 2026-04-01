cmake_minimum_required(VERSION 3.20)

project(cnerium_server
    VERSION 0.3.0
    DESCRIPTION "Cnerium Server - HTTP server layer for the Cnerium web framework"
    LANGUAGES CXX
)

include(GNUInstallDirs)

# ========================
# Options
# ========================
option(CNERIUM_SERVER_BUILD_TESTS "Build server tests" ON)
option(CNERIUM_SERVER_BUILD_EXAMPLES "Build server examples" ON)

# ========================
# Global settings
# ========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ========================
# Dependencies
# ========================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake")
endif()

if(NOT TARGET cnerium::http AND NOT TARGET cnerium_http)
    find_package(cnerium_http CONFIG QUIET)
    find_package(http CONFIG QUIET)
endif()

if(NOT TARGET cnerium::middleware AND NOT TARGET cnerium_middleware)
    find_package(cnerium_middleware CONFIG QUIET)
    find_package(middleware CONFIG QUIET)
endif()

if(NOT TARGET cnerium::router AND NOT TARGET cnerium_router)
    find_package(cnerium_router CONFIG QUIET)
    find_package(router CONFIG QUIET)
endif()

if(NOT TARGET cnerium::json AND NOT TARGET cnerium_json)
    find_package(cnerium_json CONFIG QUIET)
    find_package(json CONFIG QUIET)
endif()

set(CNERIUM_SERVER_HTTP_TARGET "")
set(CNERIUM_SERVER_MIDDLEWARE_TARGET "")
set(CNERIUM_SERVER_ROUTER_TARGET "")
set(CNERIUM_SERVER_JSON_TARGET "")

if(TARGET cnerium::http)
    set(CNERIUM_SERVER_HTTP_TARGET cnerium::http)
elseif(TARGET cnerium_http)
    set(CNERIUM_SERVER_HTTP_TARGET cnerium_http)
endif()

if(TARGET cnerium::middleware)
    set(CNERIUM_SERVER_MIDDLEWARE_TARGET cnerium::middleware)
elseif(TARGET cnerium_middleware)
    set(CNERIUM_SERVER_MIDDLEWARE_TARGET cnerium_middleware)
endif()

if(TARGET cnerium::router)
    set(CNERIUM_SERVER_ROUTER_TARGET cnerium::router)
elseif(TARGET cnerium_router)
    set(CNERIUM_SERVER_ROUTER_TARGET cnerium_router)
endif()

if(TARGET cnerium::json)
    set(CNERIUM_SERVER_JSON_TARGET cnerium::json)
elseif(TARGET cnerium_json)
    set(CNERIUM_SERVER_JSON_TARGET cnerium_json)
endif()

if(CNERIUM_SERVER_HTTP_TARGET STREQUAL "")
    message(FATAL_ERROR
        "cnerium http target not found.\n"
        "Install dependencies with Vix:\n"
        "  vix add @cnerium/http\n"
        "  vix install"
    )
endif()

if(CNERIUM_SERVER_MIDDLEWARE_TARGET STREQUAL "")
    message(FATAL_ERROR
        "cnerium middleware target not found.\n"
        "Install dependencies with Vix:\n"
        "  vix add @cnerium/middleware\n"
        "  vix install"
    )
endif()

if(CNERIUM_SERVER_ROUTER_TARGET STREQUAL "")
    message(FATAL_ERROR
        "cnerium router target not found.\n"
        "Install dependencies with Vix:\n"
        "  vix add @cnerium/router\n"
        "  vix install"
    )
endif()

if(CNERIUM_SERVER_JSON_TARGET STREQUAL "")
    message(FATAL_ERROR
        "cnerium json target not found.\n"
        "Install dependencies with Vix:\n"
        "  vix add @cnerium/json\n"
        "  vix install"
    )
endif()

# ========================
# Library (header-only)
# ========================
add_library(cnerium_server INTERFACE)
add_library(cnerium::server ALIAS cnerium_server)

target_compile_features(cnerium_server INTERFACE cxx_std_20)

target_link_libraries(cnerium_server
    INTERFACE
        ${CNERIUM_SERVER_HTTP_TARGET}
        ${CNERIUM_SERVER_MIDDLEWARE_TARGET}
        ${CNERIUM_SERVER_ROUTER_TARGET}
        ${CNERIUM_SERVER_JSON_TARGET}
)

target_include_directories(cnerium_server
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# ========================
# Compiler options
# ========================
if(MSVC)
    target_compile_options(cnerium_server INTERFACE /W4 /permissive-)
else()
    target_compile_options(cnerium_server INTERFACE -Wall -Wextra -Wpedantic)
endif()

# ========================
# Target properties
# ========================
set_target_properties(cnerium_server PROPERTIES
    EXPORT_NAME server
)

# ========================
# Examples
# ========================
if(CNERIUM_SERVER_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
endif()

# ========================
# Tests
# ========================
if(CNERIUM_SERVER_BUILD_TESTS)
    include(CTest)
    enable_testing()

    function(cnerium_add_server_test target source_file test_name)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${source_file}")
            add_executable(${target} "tests/${source_file}")
            target_link_libraries(${target} PRIVATE cnerium::server)
            target_compile_features(${target} PRIVATE cxx_std_20)

            if(MSVC)
                target_compile_options(${target} PRIVATE /W4 /permissive-)
            else()
                target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
            endif()

            add_test(NAME ${test_name} COMMAND ${target})
        endif()
    endfunction()

    cnerium_add_server_test(cnerium_server_test_routing       test_routing.cpp       cnerium_server.routing)
    cnerium_add_server_test(cnerium_server_test_params        test_params.cpp        cnerium_server.params)
    cnerium_add_server_test(cnerium_server_test_middleware    test_middleware.cpp    cnerium_server.middleware)
    cnerium_add_server_test(cnerium_server_test_not_found     test_not_found.cpp     cnerium_server.not_found)
    cnerium_add_server_test(cnerium_server_test_error_handler test_error_handler.cpp cnerium_server.error_handler)
endif()

# ========================
# Summary
# ========================
message(STATUS "------------------------------------------------------")
message(STATUS "cnerium::server configured (${PROJECT_VERSION})")
message(STATUS "HTTP target:        ${CNERIUM_SERVER_HTTP_TARGET}")
message(STATUS "Middleware target:  ${CNERIUM_SERVER_MIDDLEWARE_TARGET}")
message(STATUS "Router target:      ${CNERIUM_SERVER_ROUTER_TARGET}")
message(STATUS "JSON target:        ${CNERIUM_SERVER_JSON_TARGET}")
message(STATUS "Include dir:        ${CMAKE_CURRENT_SOURCE_DIR}/include")
message(STATUS "Build tests:        ${CNERIUM_SERVER_BUILD_TESTS}")
message(STATUS "Build examples:     ${CNERIUM_SERVER_BUILD_EXAMPLES}")
message(STATUS "Build type:         ${CMAKE_BUILD_TYPE}")
message(STATUS "------------------------------------------------------")
