cmake_minimum_required(VERSION 3.25)

project(toolchain_workspace NONE)

option(BUILD_FIRMWARE "Build embedded firmware target" OFF)

if(BUILD_FIRMWARE)
    add_subdirectory(firmware)
endif()
