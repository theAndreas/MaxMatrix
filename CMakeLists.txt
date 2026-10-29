# Host-side tests against the Arduino stand-in in test/mock/, built once per transfer path.
#
#   cmake --workflow --preset default
#
# In the root rather than under test/, so that compile_commands.json covers the library itself and
# the simulator can join later. The Arduino IDE and arduino-lint ignore the file.
cmake_minimum_required(VERSION 3.25)
project(MaxMatrix LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The library and the mock are built as gnu++11, which is what avr-gcc compiles a sketch with, so a
# construct the Uno cannot take fails here too. Only the tests are C++17, because GoogleTest 1.17
# needs it.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# -Werror here and not in a sketch build: the library is compiled with the same flags as the tests,
# so a new warning fails the tests rather than waiting for someone to read the compiler output.
add_compile_options(-Wall -Wextra -Werror)

find_package(GTest REQUIRED)
include(GoogleTest)
enable_testing()

add_library(ArduinoMock STATIC test/mock/Arduino.cpp)
target_include_directories(ArduinoMock PUBLIC test/mock)

# One library and one test binary per variant, because the two transfer paths are separate code.
function(add_maxmatrix_variant Variant)
    add_library(MaxMatrix-${Variant} STATIC MaxMatrix.cpp)
    target_include_directories(MaxMatrix-${Variant} PUBLIC ${PROJECT_SOURCE_DIR})
    target_compile_definitions(MaxMatrix-${Variant} PUBLIC ${ARGN})
    target_link_libraries(MaxMatrix-${Variant} PUBLIC ArduinoMock)

    add_executable(MaxMatrixTest-${Variant} test/MaxMatrixTest.cpp)
    set_target_properties(MaxMatrixTest-${Variant} PROPERTIES CXX_STANDARD 17)
    target_link_libraries(MaxMatrixTest-${Variant} PRIVATE MaxMatrix-${Variant} GTest::gtest_main)
    # NO_PRETTY_VALUES keeps the names the tests give their parameters - /90 rather than the enum's /1.
    gtest_discover_tests(MaxMatrixTest-${Variant} TEST_PREFIX "${Variant}/" NO_PRETTY_VALUES)
endfunction()

add_maxmatrix_variant(bitbang)
add_maxmatrix_variant(spi MAXMATRIX_USE_SPI=STD_ON)
