cmake_minimum_required(VERSION 3.20)

project(FishRPC CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES src/*.cpp)

add_executable(coroutine_test test/test_coroutine.cpp)
target_include_directories(coroutine_test PRIVATE ./include)

add_executable(event_loop_test test/test_event_loop.cpp)
target_include_directories(event_loop_test PRIVATE ./include)
target_link_libraries(event_loop_test uring FishRPC)

add_executable(protocol_test test/test_protocol.cpp)
target_include_directories(protocol_test PRIVATE ./include)
target_link_libraries(protocol_test uring fishrpc_proto FishRPC)



#==============================================================
# ---------------- protobuf ----------------
find_package(Protobuf REQUIRED)

file(GLOB PROTO_FILES CONFIGURE_DEPENDS proto/*.proto)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(fishrpc_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(fishrpc_proto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)
target_link_libraries(fishrpc_proto PUBLIC protobuf::libprotobuf)

add_library(FishRPC ${SOURCES})
target_include_directories(FishRPC PUBLIC ./include)
target_link_libraries(FishRPC PUBLIC uring fishrpc_proto)

add_executable(write_frame_test test/test_write_frame.cpp)
target_include_directories(write_frame_test PRIVATE ./include)
target_link_libraries(write_frame_test uring fishrpc_proto FishRPC)

add_executable(read_frame_test test/test_read_frame.cpp)
target_include_directories(read_frame_test PRIVATE ./include)
target_link_libraries(read_frame_test uring fishrpc_proto FishRPC)

add_executable(server_test test/test_server.cpp)
target_include_directories(server_test PRIVATE ./include)
target_link_libraries(server_test uring fishrpc_proto FishRPC)

add_executable(client_test test/test_client.cpp)
target_include_directories(client_test PRIVATE ./include)
target_link_libraries(client_test uring fishrpc_proto FishRPC)


set(test_name 
  coroutine_test
  event_loop_test
  protocol_test
  write_frame_test
  read_frame_test
  server_test
  client_test
)

foreach(t IN LISTS test_name )
  add_test(NAME ${t} COMMAND ${t})
  set_tests_properties(${t} PROPERTIES TIMEOUT 30)
endforeach()


add_executable(example_server ./examples/example_server.cpp)
target_include_directories(example_server PRIVATE ./include)
target_link_libraries(example_server uring fishrpc_proto FishRPC)
