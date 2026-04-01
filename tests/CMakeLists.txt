cmake_minimum_required(VERSION 3.20)

project(cnerium_server_tests LANGUAGES CXX)

include(CTest)
enable_testing()

function(cnerium_add_test name file)
    add_executable(${name} ${file})
    target_link_libraries(${name} PRIVATE server::server)
    add_test(NAME ${name} COMMAND ${name})

    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

cnerium_add_test(test_routing test_routing.cpp)
cnerium_add_test(test_params test_params.cpp)
cnerium_add_test(test_middleware test_middleware.cpp)
cnerium_add_test(test_not_found test_not_found.cpp)
cnerium_add_test(test_error_handler test_error_handler.cpp)
