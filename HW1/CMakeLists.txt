cmake_minimum_required(VERSION 3.15)
project(HW1)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(HW1
        Commands.cpp
        Commands.h
        Makefile
        signals.cpp
        signals.h
        smash.cpp)
