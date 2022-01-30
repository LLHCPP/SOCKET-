cmake_minimum_required(VERSION 3.16)
project(SERVER)

set(CMAKE_CXX_STANDARD 14)

add_executable(SERVER main.cpp ThreadPoll.hpp)
TARGET_LINK_LIBRARIES(SERVER pthread)
