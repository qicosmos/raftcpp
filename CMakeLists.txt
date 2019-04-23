cmake_minimum_required(VERSION 3.7)
project(raftcpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++17")

find_package(Boost COMPONENTS system REQUIRED)
include_directories(
        "./rest_rpc/include"
        "./rest_rpc/third/msgpack/include"
	"./iguana"
)

add_executable(raftcpp main.cpp)
target_link_libraries(raftcpp ${Boost_LIBRARIES} -lstdc++fs)
