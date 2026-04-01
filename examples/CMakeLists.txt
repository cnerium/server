cmake_minimum_required(VERSION 3.20)

project(cnerium_server_examples LANGUAGES CXX)

# ========================
# Helpers
# ========================
function(cnerium_add_server_example target source_file)
    add_executable(${target} ${source_file})
    target_link_libraries(${target} PRIVATE server::server)
    target_compile_features(${target} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ========================
# Examples
# ========================
cnerium_add_server_example(server_example_01 01_hello_server.cpp)
cnerium_add_server_example(server_example_02 02_routes_basic.cpp)
cnerium_add_server_example(server_example_03 03_json_response.cpp)
cnerium_add_server_example(server_example_04 04_middleware_pipeline.cpp)
cnerium_add_server_example(server_example_05 05_not_found_and_errors.cpp)
