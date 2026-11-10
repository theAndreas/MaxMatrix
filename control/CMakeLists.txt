# The control program: SerialControl on a board or in the simulator, from a window. Part of the root's build, which
# tests its classes, and a project of its own as well, for a system the root's tests do not build on:
#
#   cmake -S control -B build-control && cmake --build build-control
#
# Everything but the window is the library Control, without wxWidgets, so that the tests need none.
cmake_minimum_required(VERSION 3.25)
project(MaxMatrixControl LANGUAGES CXX)

add_library(Control STATIC ControlCommand.cpp ControlLink.cpp ControlSerialPort.cpp ControlTcpPort.cpp)
# src/ for MaxMatrixConfig.h: the program knows the line length and the chain SerialControl is built with by default
target_include_directories(Control PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(WIN32)
    target_link_libraries(Control PUBLIC ws2_32)
endif()

# Left out where there is no wxWidgets, as the tests of Control need none.
find_package(wxWidgets 3.2 COMPONENTS core base)
if(wxWidgets_FOUND)
    add_executable(MaxMatrixControl WIN32 main.cpp ControlWindow.cpp ControlDotPanel.cpp)
    # SYSTEM, so that a warning in wxWidgets' headers does not fail the -Werror of the root's build
    target_include_directories(MaxMatrixControl SYSTEM PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_compile_definitions(MaxMatrixControl PRIVATE ${wxWidgets_DEFINITIONS})
    target_compile_options(MaxMatrixControl PRIVATE ${wxWidgets_CXX_FLAGS})
    # wxWidgets combines flags of different enum types, wxEXPAND | wxALL, by design, and clang's
    # -Wenum-enum-conversion reports each; only the window does it, not the library Control.
    target_compile_options(MaxMatrixControl PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wno-enum-enum-conversion>)
    target_link_libraries(MaxMatrixControl PRIVATE Control ${wxWidgets_LIBRARIES})
else()
    message(STATUS "wxWidgets not found: the control program is left out")
endif()
