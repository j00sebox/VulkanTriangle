cmake_minimum_required(VERSION 3.0)
project(STB)

add_library(stb stb_image.h stb_image.cpp)
target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${SRC_DIR})