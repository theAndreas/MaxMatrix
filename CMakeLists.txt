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

add_library(ArduinoMock STATIC test/mock/Arduino.cpp test/mock/Max7219.cpp)
target_include_directories(ArduinoMock PUBLIC test/mock)

# The MAX7219 model on its own, once: it does not depend on the transfer path.
add_executable(Max7219Test test/Max7219Test.cpp)
set_target_properties(Max7219Test PROPERTIES CXX_STANDARD 17)
target_link_libraries(Max7219Test PRIVATE ArduinoMock GTest::gtest_main)
gtest_discover_tests(Max7219Test TEST_PREFIX "model/")

# One library and one test binary per variant, because the two transfer paths are separate code.
function(add_maxmatrix_variant Variant)
    add_library(MaxMatrix-${Variant} STATIC ${LibrarySources})
    target_include_directories(MaxMatrix-${Variant} PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_compile_definitions(MaxMatrix-${Variant} PUBLIC ${ARGN})
    target_link_libraries(MaxMatrix-${Variant} PUBLIC ArduinoMock)

    add_executable(MaxMatrixTest-${Variant} test/MaxMatrixTest.cpp)
    set_target_properties(MaxMatrixTest-${Variant} PROPERTIES CXX_STANDARD 17)
    target_link_libraries(MaxMatrixTest-${Variant} PRIVATE MaxMatrix-${Variant} GTest::gtest_main)
    # NO_PRETTY_VALUES keeps the names the tests give their parameters - /90 rather than the enum's /1.
    gtest_discover_tests(MaxMatrixTest-${Variant} TEST_PREFIX "${Variant}/" NO_PRETTY_VALUES)

    # One simulator per example, the sketch compiled unchanged: as the Arduino IDE does, as C++ with Arduino.h
    # included first. gnu++11 like the library, so a sketch that would not build for the Uno fails here as well.
    foreach(Sketch ${Examples})
        get_filename_component(Example ${Sketch} NAME_WE)
        add_executable(Simulator-${Example}-${Variant} simulator/Simulator.cpp simulator/WebHost.cpp ${Sketch})
        target_link_libraries(Simulator-${Example}-${Variant} PRIVATE MaxMatrix-${Variant})
        # read at run time, so that a change to the page needs a reload in the browser rather than a rebuild
        target_compile_definitions(Simulator-${Example}-${Variant} PRIVATE
                                   SIMULATOR_PAGE="${PROJECT_SOURCE_DIR}/simulator/index.html")
    endforeach()

    # Basics shows "Hi!" for its first two seconds; one second in, the panel of an FC-16 board has to read it.
    add_test(NAME ${Variant}/simulator/Basics
             COMMAND Simulator-Basics-${Variant} --for 1 --expect ${PROJECT_SOURCE_DIR}/simulator/Basics.txt)
    # Off 8080, where a developer's own simulator runs, and one variant at a time, since both need the port.
    add_test(NAME ${Variant}/simulator/web
             COMMAND bash ${PROJECT_SOURCE_DIR}/simulator/WebTest.sh $<TARGET_FILE:Simulator-Basics-${Variant}> 18080)
    set_tests_properties(${Variant}/simulator/web PROPERTIES RESOURCE_LOCK SimulatorWebPort)
endfunction()

# The library as the Arduino IDE builds it: everything under src/, each part in a folder of its own.
file(GLOB_RECURSE LibrarySources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB Examples ${PROJECT_SOURCE_DIR}/examples/*/*.ino)
set_source_files_properties(${Examples} PROPERTIES LANGUAGE CXX COMPILE_OPTIONS "-include;Arduino.h")

add_maxmatrix_variant(bitbang)
add_maxmatrix_variant(spi MAXMATRIX_USE_SPI=STD_ON)
