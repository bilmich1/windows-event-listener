cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(/W4 /WX /permissive- /bigobj)

project(windows-event-listener)

find_package(Boost REQUIRED COMPONENTS system)

add_subdirectory(src)
