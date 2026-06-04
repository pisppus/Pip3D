cmake_minimum_required(VERSION 3.21)

project(Pip3DSimulator LANGUAGES CXX)

option(SIM_DEBUG "Build the simulator executable with debug compiler flags." OFF)

if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

if(NOT DEFINED SIM_ROOT)
    get_filename_component(SIM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
get_filename_component(SIM_ROOT "${SIM_ROOT}" ABSOLUTE)
file(TO_CMAKE_PATH "${SIM_ROOT}" SIM_ROOT)

file(GLOB_RECURSE SIM_LIB_SOURCES
    "${SIM_ROOT}/lib/Pip3D/*.cpp")
file(GLOB SIM_APP_SOURCES
    "${SIM_ROOT}/src/*.cpp")

set(SIM_SOURCES ${SIM_LIB_SOURCES} ${SIM_APP_SOURCES})
list(FILTER SIM_SOURCES EXCLUDE REGEX ".*/PipCore/Platforms/ESP32/.*")
list(FILTER SIM_SOURCES EXCLUDE REGEX ".*/PipCore/Displays/ST7789/.*")
list(FILTER SIM_SOURCES EXCLUDE REGEX ".*/PipCore/Displays/ILI9488/.*")

if(SIM_DEBUG)
    if(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "/MD /Zi /Ob0 /Od" CACHE STRING "" FORCE)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG" CACHE STRING "" FORCE)
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "-O0 -g" CACHE STRING "" FORCE)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "" CACHE STRING "" FORCE)
    endif()
endif()

add_executable(simulator WIN32 ${SIM_SOURCES})
target_compile_features(simulator PRIVATE cxx_std_17)
target_compile_definitions(simulator PRIVATE
    PIPCORE_PLATFORM=DESKTOP
    PIPCORE_DISPLAY=SIMULATOR
    PIP3D_PC=1
    PIPGUI_SIM_USE_WX=1
    PIPGUI_SIM_PROJECT_NAME=\"Pip3D\"
    PIPGUI_SIM_DEFAULT_WIDTH=320
    PIPGUI_SIM_DEFAULT_HEIGHT=240
    WINVER=0x0601
    _WIN32_WINNT=0x0601
    WIN32_LEAN_AND_MEAN
    NOMINMAX)
target_compile_options(simulator PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FIArduino.h /EHsc>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include Arduino.h>)
if(SIM_DEBUG)
    target_compile_definitions(simulator PRIVATE DEBUG)
    target_link_options(simulator PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/DEBUG>)
    set(SIM_OUTPUT_NAME "simulator-debug")
else()
    set(SIM_OUTPUT_NAME "simulator")
endif()
target_include_directories(simulator PRIVATE
    "${SIM_ROOT}/include"
    "${SIM_ROOT}/lib/Pip3D/PipCore/Host/Desktop/include"
    "${SIM_ROOT}/lib/Pip3D"
    "${SIM_ROOT}/lib/Pip3D/Pip3D")

set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release)
set(CMAKE_MAP_IMPORTED_CONFIG_PIPDEBUG Release)
find_package(wxWidgets CONFIG QUIET)
if(wxWidgets_FOUND)
    target_link_libraries(simulator PRIVATE wx::core wx::base)
else()
    find_package(wxWidgets REQUIRED COMPONENTS core base)
    include(${wxWidgets_USE_FILE})
    target_link_libraries(simulator PRIVATE ${wxWidgets_LIBRARIES})
endif()

if(WIN32)
    target_link_libraries(simulator PRIVATE
        user32 gdi32 windowscodecs advapi32 dwmapi
        mfplat mfreadwrite mfuuid ole32)
endif()

set_target_properties(simulator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SIM_ROOT}/.sim"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${SIM_ROOT}/.sim"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${SIM_ROOT}/.sim"
    OUTPUT_NAME "${SIM_OUTPUT_NAME}")
