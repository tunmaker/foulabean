#src/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

project(digitwin VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(main)
add_subdirectory(renodeAPI)