cmake_minimum_required(VERSION 3.20)

project(FishRPC CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES src/*.cpp)

# add_library(FishRPC ${SOURCES} )

#target_include_directories(FishRPC PRIVATE ./include)

add_executable(coroutine_test test/test_coroutine.cpp)
target_include_directories(coroutine_test PRIVATE ./include)
