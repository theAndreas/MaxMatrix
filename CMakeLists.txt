# Host-side tests against the Arduino stand-in in test/mock/, built once per transfer path.
#
#   make tests      # cmake --workflow --preset default, and the presets clang, sanitize, coverage
#   make avr        # the examples as the Uno runs them, in simavr: MAXMATRIX_AVR_TESTS
#
# In the root rather than under test/, so that compile_commands.json covers the library itself and
# the simulator can join later. The Arduino IDE and arduino-lint ignore the file. The presets come
# from the template; the warnings and their instrumentation from cmake/checks.cmake.
cmake_minimum_required(VERSION 3.25)
project(MaxMatrix LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(cmake/checks.cmake)
option(MAXMATRIX_AVR_TESTS "Build every example for the Uno and run it in simavr" OFF)

# The library and the mock are built as gnu++11, which is what avr-gcc compiles a sketch with, so a
# construct the Uno cannot take fails here too. Only the tests are C++17, because GoogleTest 1.17
# needs it.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# The template's warnings for everything built here, the library, the tests, the simulator and the
# control program, as the library is compiled with the same flags as the tests. Errors where
# CMAKE_COMPILE_WARNING_AS_ERROR is on, as the ci- presets and `make check` set it. The
# instrumentation of the presets sanitize and coverage likewise.
add_compile_options(${CHECK_WARNINGS} ${CHECK_INSTRUMENT})
add_link_options(${CHECK_INSTRUMENT})

find_package(GTest REQUIRED)
include(GoogleTest)
enable_testing()

# The model of the MAX7219 chain, which the stand-in drives: our own code, so its headers are
# checked as every other of the tests.
add_library(Max7219Model STATIC test/model/Max7219.cpp)
# src/ for MaxMatrixAttributes.h alone, which marks a result that must not be dropped (FN-10) and needs nothing else.
target_include_directories(Max7219Model PUBLIC test/model src)

add_library(ArduinoMock STATIC test/mock/Arduino.cpp)
# SYSTEM, as the board takes the core it stands in for: neither the compiler nor clang-tidy
# reports what lies in its headers, as the core's binary.h and bitRead(), which are the core's
# and not this repository's. Its sources are compiled with every warning still.
target_include_directories(ArduinoMock SYSTEM PUBLIC test/mock)
target_link_libraries(ArduinoMock PUBLIC Max7219Model)

# The MAX7219 model on its own, once: it does not depend on the transfer path.
add_executable(Max7219Test test/Max7219Test.cpp)
set_target_properties(Max7219Test PROPERTIES CXX_STANDARD 17)
target_link_libraries(Max7219Test PRIVATE Max7219Model GTest::gtest_main)
gtest_discover_tests(Max7219Test TEST_PREFIX "model/")

# The simulator's hosts once as well, the web host and the serial port on TCP: they know neither the library nor the
# sketch, and the WebSocket half is tested on its own, without a network.
add_library(SimulatorHosts STATIC simulator/Host/SimulatorSocket.cpp simulator/Host/SimulatorWebHost.cpp
                                  simulator/Host/SimulatorWebSocket.cpp simulator/Host/SimulatorSerialHost.cpp)
# src/ for MaxMatrixAttributes.h alone, as for the model.
target_include_directories(SimulatorHosts PUBLIC simulator src)
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
# The tests of the library, a file per header as in src/ (TS-5 of docs/Testing.md), so that clang-tidy reads
# them side by side; what they share is in MaxMatrixTestSupport.
set(MaxMatrixTestSources
    test/MaxMatrixTestSupport.cpp
    test/MaxMatrixTest.cpp
    test/MaxMatrixChainLengthTest.cpp
    test/MaxMatrixErrorTest.cpp
    test/Command/CommandTest.cpp
    test/Font/FontTest.cpp
    test/Matrix/PanelTest.cpp
    test/Matrix/WiredTest.cpp
    test/Max7219/ChainTest.cpp
    test/Text/TextTest.cpp)

function(add_maxmatrix_variant Variant)
    add_library(MaxMatrix-${Variant} STATIC ${LibrarySources})
    target_include_directories(MaxMatrix-${Variant} PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_compile_definitions(MaxMatrix-${Variant} PUBLIC ${ARGN})
    target_link_libraries(MaxMatrix-${Variant} PUBLIC ArduinoMock)

    add_executable(MaxMatrixTest-${Variant} ${MaxMatrixTestSources})
    set_target_properties(MaxMatrixTest-${Variant} PROPERTIES CXX_STANDARD 17)
    # MaxMatrixTestSupport.h for the tests in the folders below test/, by its name, as src/ is
    target_include_directories(MaxMatrixTest-${Variant} PRIVATE ${PROJECT_SOURCE_DIR}/test)
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
# The library's own report, which prints on Serial: in a binary of its own, as the tests above define the report
# themselves to record it, and the default is linked only where nothing else defines it.
add_executable(MaxMatrixErrorReportTest test/MaxMatrixErrorReportTest.cpp)
set_target_properties(MaxMatrixErrorReportTest PROPERTIES CXX_STANDARD 17)
target_link_libraries(MaxMatrixErrorReportTest PRIVATE MaxMatrix-det GTest::gtest_main)
gtest_discover_tests(MaxMatrixErrorReportTest TEST_PREFIX "det/")

# What the compiler has to report: a getter's result dropped, whose value is lost and the error that says whether there
# was one (FN-10). In both standards, as MAXMATRIX_NODISCARD is another attribute in each; with code generated, as GCC
# reports warn_unused_result only then, not under -fsyntax-only. The case is written into the build folder rather than
# kept in test/, whose every file clang-tidy reads from the database of the tests, where a file that must warn has no
# place.
set(DroppedResultCase ${CMAKE_CURRENT_BINARY_DIR}/DroppedGetColumn.cpp)
file(CONFIGURE OUTPUT ${DroppedResultCase}
     CONTENT "#include \"MaxMatrix.h\"\nvoid DropColumn(max_matrix::Matrix& matrix) { matrix.GetColumn(0); }\n")
foreach(Standard gnu++11 c++17)
    add_test(NAME nodiscard/DroppedGetColumn/${Standard}
             COMMAND ${CMAKE_CXX_COMPILER} -std=${Standard} -c -o ${CMAKE_CURRENT_BINARY_DIR}/DroppedGetColumn-${Standard}.o
                     -I${PROJECT_SOURCE_DIR}/src -isystem ${PROJECT_SOURCE_DIR}/test/mock -I${PROJECT_SOURCE_DIR}/test/model
                     ${DroppedResultCase})
    set_tests_properties(nodiscard/DroppedGetColumn/${Standard}
                         PROPERTIES PASS_REGULAR_EXPRESSION "ignoring return value[^\n]*-Wunused-result")
endforeach()

# The pictures in docu/ are drawn from the library and the model rather than by hand: DocuPictures writes them, and the
# test fails when one in docu/ is no longer what the library would draw. Once, as the transfer path does not matter.
add_executable(DocuPictures docu/DocuPictures.cpp docu/DocuSvg.cpp)
target_link_libraries(DocuPictures PRIVATE MaxMatrix-bitbang)
add_test(NAME docu/pictures COMMAND DocuPictures --check ${PROJECT_SOURCE_DIR}/docu)

# The examples as the Uno runs them: the .elf avr-gcc makes of each, with bit banging and with SPI, in simavr against the
# MAX7219 model. Both transfer paths have to light what the one expectation of an example says, at a simulated time,
# which ends the same every run, so that a moving example is checked as well. Needs arduino-cli with the AVR core and
# simavr. Only with MAXMATRIX_AVR_TESTS, which `make avr` sets in a build of its own: the presets
# of the host would compile all examples twice each again, and their compiler flags, coverage or
# the sanitizers, do not reach avr-gcc anyway.
#
# SimulatorAvr itself is built wherever simavr is, so that the compilation database has it for
# clang-tidy and clangd; the tests need MAXMATRIX_AVR_TESTS as well, and fail without simavr.
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(Simavr IMPORTED_TARGET simavr)
endif()
if(Simavr_FOUND)
    add_executable(SimulatorAvr simulator/Avr/mainAvr.cpp simulator/Avr/SimulatorAvr.cpp)
    target_link_libraries(SimulatorAvr PRIVATE Simulator PkgConfig::Simavr)
elseif(MAXMATRIX_AVR_TESTS)
    message(FATAL_ERROR "MAXMATRIX_AVR_TESTS needs simavr's library, see .devcontainer/apt-packages.txt")
endif()
if(MAXMATRIX_AVR_TESTS)
    find_program(ArduinoCli arduino-cli REQUIRED)

    # an example, the simulated seconds it runs for, and the --serial lines it is sent; its expectation is
    # simulator/Expected/EXAMPLE.txt, the same as for the simulator a sketch is built into, where there is one.
    # Sprites steps by delay() plus the time its frames take, which simavr's SPI takes longer for than bit banging, so
    # the two drift apart a step at a time; at 0.48 s, in the middle of a window of 0.45 to 0.51 s, both show the same.
    set(AvrRuns "Basics|1|" "ScrollText|2|" "Messages|3|" "Animation|1|" "Sprites|0.48|" "Shapes|2.5|" "Shutdown|5|" "Band|1|" "SerialControl|1|text=AVR")
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
endif()
