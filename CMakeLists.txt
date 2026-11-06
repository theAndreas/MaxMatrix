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

# The simulator's hosts once as well, the web host and the serial port on TCP: they know neither the library nor the
# sketch, and the WebSocket half is tested on its own, without a network.
add_library(SimulatorHosts STATIC simulator/Host/SimulatorSocket.cpp simulator/Host/SimulatorWebHost.cpp
                                  simulator/Host/SimulatorWebSocket.cpp simulator/Host/SimulatorSerialHost.cpp)
target_include_directories(SimulatorHosts PUBLIC simulator)
foreach(Test SimulatorWebSocketTest SimulatorWebHostTest SimulatorSerialHostTest)
    add_executable(${Test} test/${Test}.cpp)
    set_target_properties(${Test} PROPERTIES CXX_STANDARD 17)
    target_link_libraries(${Test} PRIVATE SimulatorHosts GTest::gtest_main)
    # one port per test binary, and ctest runs every test of it as a process of its own
    gtest_discover_tests(${Test} TEST_PREFIX "simulator/" PROPERTIES RESOURCE_LOCK ${Test})
endforeach()

# The rest of the simulator but main.cpp once too: it reads the model, never the library, so the transfer path makes no
# difference to it, and its tests need no sketch.
add_library(Simulator STATIC simulator/SimulatorOptions.cpp simulator/SimulatorPanel.cpp simulator/SimulatorTerminal.cpp
                             simulator/Web/SimulatorWeb.cpp simulator/SimulatorFrontEnd.cpp)
target_include_directories(Simulator PUBLIC src)
target_link_libraries(Simulator PUBLIC SimulatorHosts ArduinoMock)
# read at run time, so that a change to the page needs a reload in the browser rather than a rebuild
target_compile_definitions(Simulator PRIVATE SIMULATOR_PAGE="${PROJECT_SOURCE_DIR}/simulator/Web/index.html")
add_executable(SimulatorTest test/SimulatorTest.cpp)
set_target_properties(SimulatorTest PROPERTIES CXX_STANDARD 17)
target_link_libraries(SimulatorTest PRIVATE Simulator GTest::gtest_main)
gtest_discover_tests(SimulatorTest TEST_PREFIX "simulator/")

# The control program, and its classes without a window once: they know neither the library nor the sketch. Its ports
# are tested against a pseudo terminal and against the simulator's serial host, on a port of its own.
add_subdirectory(control)
foreach(Test ControlCommandTest ControlLinkTest ControlPortTest)
    add_executable(${Test} test/${Test}.cpp)
    set_target_properties(${Test} PROPERTIES CXX_STANDARD 17)
    target_link_libraries(${Test} PRIVATE Control SimulatorHosts GTest::gtest_main)
    gtest_discover_tests(${Test} TEST_PREFIX "control/" PROPERTIES RESOURCE_LOCK ${Test})
endforeach()

# The script of the simulator's page, against stand-ins for the DOM, the WebSocket and a board on Web Serial. Node is no
# part of the dev container's Dockerfile, so the test is left out where there is none; the CI runners have it.
find_program(NodeExecutable node)
if(NodeExecutable)
    add_test(NAME simulator/page COMMAND ${NodeExecutable} ${PROJECT_SOURCE_DIR}/simulator/Test/PageTest.js
                                         ${PROJECT_SOURCE_DIR}/simulator/Web/index.html)
else()
    message(STATUS "node not found: the test simulator/page is left out")
endif()

# One library and one test binary per variant: the two transfer paths are separate code, and the reports of
# MAXMATRIX_DEV_ERROR_DETECT are compiled in only where it is on.
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
endfunction()

# The examples and the simulator for a variant of the library.
function(add_simulator_variant Variant)
    # One simulator per example, the sketch compiled unchanged: as the Arduino IDE does, as C++ with Arduino.h
    # included first. gnu++11 like the library, so a sketch that would not build for the Uno fails here as well.
    foreach(Sketch ${Examples})
        get_filename_component(Example ${Sketch} NAME_WE)
        add_executable(Simulator-${Example}-${Variant} simulator/main.cpp ${Sketch})
        target_link_libraries(Simulator-${Example}-${Variant} PRIVATE MaxMatrix-${Variant} Simulator)
        # Every example for two seconds: it builds against the mock, and every frame it sends reaches the chain whole.
        # What it draws depends on the host's clock, so only Basics, which stands still at first, is compared.
        add_test(NAME ${Variant}/simulator/run/${Example} COMMAND Simulator-${Example}-${Variant} --for 2)
    endforeach()

    # Basics shows "Hi!" for its first two seconds; one second in, the panel of an FC-16 board has to read it.
    add_test(NAME ${Variant}/simulator/Basics
             COMMAND Simulator-Basics-${Variant} --for 1 --expect ${PROJECT_SOURCE_DIR}/simulator/Expected/Basics.txt)
    # Off 8080, where a developer's own simulator runs, and one variant at a time, since both need the port.
    add_test(NAME ${Variant}/simulator/web
             COMMAND bash ${PROJECT_SOURCE_DIR}/simulator/Test/WebTest.sh $<TARGET_FILE:Simulator-Basics-${Variant}> 18080)
    set_tests_properties(${Variant}/simulator/web PROPERTIES RESOURCE_LOCK SimulatorWebPort)
    # SerialControl controlled the way the control page does it, on a port of its own.
    add_test(NAME ${Variant}/simulator/control
             COMMAND bash ${PROJECT_SOURCE_DIR}/simulator/Test/ControlTest.sh $<TARGET_FILE:Simulator-SerialControl-${Variant}> 18082)
    set_tests_properties(${Variant}/simulator/control PROPERTIES RESOURCE_LOCK SimulatorControlPort)
    # SerialControl controlled the way the control program does it, over its Serial on a TCP port.
    add_test(NAME ${Variant}/simulator/tcp
             COMMAND bash ${PROJECT_SOURCE_DIR}/simulator/Test/TcpTest.sh $<TARGET_FILE:Simulator-SerialControl-${Variant}> 18084)
    set_tests_properties(${Variant}/simulator/tcp PROPERTIES RESOURCE_LOCK SimulatorTcpPort)
endfunction()

# The library as the Arduino IDE builds it: everything under src/, each part in a folder of its own.
file(GLOB_RECURSE LibrarySources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB Examples ${PROJECT_SOURCE_DIR}/examples/*/*.ino)
set_source_files_properties(${Examples} PROPERTIES LANGUAGE CXX COMPILE_OPTIONS "-include;Arduino.h")

add_maxmatrix_variant(bitbang)
add_maxmatrix_variant(spi MAXMATRIX_USE_SPI=STD_ON)
add_simulator_variant(bitbang)
add_simulator_variant(spi)
# With the reports on, once and without the simulators: the tests check every report, and the transfer path makes no
# difference to them.
add_maxmatrix_variant(det MAXMATRIX_DEV_ERROR_DETECT=STD_ON)

# The pictures in docu/ are drawn from the library and the model rather than by hand: DocuPictures writes them, and the
# test fails when one in docu/ is no longer what the library would draw. Once, as the transfer path does not matter.
add_executable(DocuPictures docu/DocuPictures.cpp docu/DocuSvg.cpp)
target_link_libraries(DocuPictures PRIVATE MaxMatrix-bitbang)
add_test(NAME docu/pictures COMMAND DocuPictures --check ${PROJECT_SOURCE_DIR}/docu)

# The examples as the Uno runs them: the .elf avr-gcc makes of each, with bit banging and with SPI, in simavr against the
# MAX7219 model. Both transfer paths have to light what the one expectation of an example says, at a simulated time,
# which ends the same every run, so that a moving example is checked as well. Needs arduino-cli with the AVR core and
# simavr; left out where either is missing, as in the host-tests job of CI.
find_program(ArduinoCli arduino-cli)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(Simavr IMPORTED_TARGET simavr)
endif()
if(ArduinoCli AND Simavr_FOUND)
    add_executable(SimulatorAvr simulator/Avr/mainAvr.cpp simulator/Avr/SimulatorAvr.cpp)
    target_link_libraries(SimulatorAvr PRIVATE Simulator PkgConfig::Simavr)

    # an example, the simulated seconds it runs for, and the --serial lines it is sent; its expectation is
    # simulator/Expected/EXAMPLE.txt, the same as for the simulator a sketch is built into, where there is one
    set(AvrRuns "Basics|1|" "ScrollText|2|" "Messages|3|" "Animation|1|" "Sprites|1.5|" "SerialControl|1|text=AVR")
    file(GLOB_RECURSE LibraryHeaders CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.h)
    set(AvrElfs)
    foreach(Transfer bitbang spi)
        set(TransferFlags)
        if(Transfer STREQUAL "spi")
            set(TransferFlags --build-property "compiler.cpp.extra_flags=-DMAXMATRIX_USE_SPI=STD_ON")
        endif()
        foreach(Run ${AvrRuns})
            string(REPLACE "|" ";" Run "${Run}")
            list(GET Run 0 Example)
            list(GET Run 1 Seconds)
            list(GET Run 2 Serial)
            set(Directory ${CMAKE_BINARY_DIR}/avr/${Transfer}/${Example})
            add_custom_command(OUTPUT ${Directory}/${Example}.ino.elf
                               COMMAND ${ArduinoCli} compile --fqbn arduino:avr:uno --library ${PROJECT_SOURCE_DIR}
                                       ${TransferFlags} --build-path ${Directory}/build --output-dir ${Directory}
                                       ${PROJECT_SOURCE_DIR}/examples/${Example}
                               DEPENDS ${PROJECT_SOURCE_DIR}/examples/${Example}/${Example}.ino ${LibrarySources}
                                       ${LibraryHeaders}
                               COMMENT "arduino-cli: ${Example} for the Uno, ${Transfer}")
            list(APPEND AvrElfs ${Directory}/${Example}.ino.elf)
            set(SerialArguments)
            if(Serial)
                set(SerialArguments --serial ${Serial})
            endif()
            add_test(NAME avr/${Transfer}/${Example}
                     COMMAND SimulatorAvr --elf ${Directory}/${Example}.ino.elf ${SerialArguments} --for ${Seconds}
                             --expect ${PROJECT_SOURCE_DIR}/simulator/Expected/${Example}.txt)
        endforeach()
        # SerialControl controlled as the page does, through the UART of the AVR; on a port of its own, one transfer
        # path at a time.
        add_test(NAME avr/${Transfer}/control
                 COMMAND bash ${PROJECT_SOURCE_DIR}/simulator/Test/ControlTest.sh $<TARGET_FILE:SimulatorAvr> 18083
                         --elf ${CMAKE_BINARY_DIR}/avr/${Transfer}/SerialControl/SerialControl.ino.elf)
        set_tests_properties(avr/${Transfer}/control PROPERTIES RESOURCE_LOCK SimulatorAvrControlPort)
        add_test(NAME avr/${Transfer}/tcp
                 COMMAND bash ${PROJECT_SOURCE_DIR}/simulator/Test/TcpTest.sh $<TARGET_FILE:SimulatorAvr> 18085
                         --elf ${CMAKE_BINARY_DIR}/avr/${Transfer}/SerialControl/SerialControl.ino.elf)
        set_tests_properties(avr/${Transfer}/tcp PROPERTIES RESOURCE_LOCK SimulatorAvrTcpPort)
    endforeach()
    add_custom_target(AvrExamples ALL DEPENDS ${AvrElfs})
else()
    message(STATUS "arduino-cli or simavr not found: the tests avr/ are left out")
endif()
