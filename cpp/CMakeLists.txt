cmake_minimum_required(VERSION 3.20)
project(REDoxCpp VERSION 1.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build REDoxCpp as a shared library (DLL/SO)" OFF)

if(WIN32 AND BUILD_SHARED_LIBS)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(REDoxCpp
    src/token.cpp
    src/utf8_helper.cpp
    src/document.cpp
    src/element.cpp
    src/container.cpp
    src/writer.cpp
    src/json_document.cpp
    src/json5_document.cpp
    src/json_writer.cpp
    src/dox_document.cpp
    src/cbor_document.cpp
    src/messagepack_document.cpp
    src/ini_document.cpp
)

add_library(REDox::REDoxCpp ALIAS REDoxCpp)

target_include_directories(REDoxCpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(REDoxCpp PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(REDoxCpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(REDOX_BUILD_TESTS "Build REDoxCpp CTest unit tests" ON)

if(REDOX_BUILD_TESTS)
    enable_testing()

    function(redox_add_test name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE REDoxCpp)
        target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        if(MSVC)
            target_compile_options(${name} PRIVATE /W4 /utf-8 /permissive-)
        else()
            target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    redox_add_test(test_token_and_dom tests/test_token_and_dom.cpp)
    redox_add_test(test_json tests/test_json.cpp)
    redox_add_test(test_json5 tests/test_json5.cpp)
    redox_add_test(test_dox tests/test_dox.cpp)
    redox_add_test(test_formats tests/test_formats.cpp)

    # Pass external conformance suite paths if submodules are present
    set(REDOX_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external")
    if(EXISTS "${REDOX_EXTERNAL_DIR}/JSONTestSuite/test_parsing")
        target_compile_definitions(test_json PRIVATE
            REDOX_JSON_TEST_SUITE_DIR="${REDOX_EXTERNAL_DIR}/JSONTestSuite/test_parsing")
    endif()
    if(EXISTS "${REDOX_EXTERNAL_DIR}/json5-tests")
        target_compile_definitions(test_json5 PRIVATE
            REDOX_JSON5_TEST_SUITE_DIR="${REDOX_EXTERNAL_DIR}/json5-tests")
    endif()
endif()
