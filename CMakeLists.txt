cmake_minimum_required(VERSION 4.1)
project(SRuntime)

set(CMAKE_CXX_STANDARD 20)

add_executable(SRuntime main.cpp
        virtualMachine.cpp
        virtualMachine.h
        instruction.h)
