cmake_minimum_required(VERSION 3.20)

project(FishRPC CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES src/*.cpp)

# add_library(FishRPC ${SOURCES} )

#target_include_directories(FishRPC PRIVATE ./include)

add_executable(coroutine_test test/test_coroutine.cpp)
target_include_directories(coroutine_test PRIVATE ./include)




# ---------------- protobuf ----------------
find_package(Protobuf REQUIRED)

file(GLOB PROTO_FILES proto/*.proto)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(fishrpc_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(fishrpc_proto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)
target_link_libraries(fishrpc_proto PUBLIC protobuf::libprotobuf)
