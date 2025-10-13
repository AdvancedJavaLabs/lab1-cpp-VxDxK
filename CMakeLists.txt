cmake_minimum_required(VERSION 3.30)
project(Bedrock)
set(CMAKE_CXX_STANDARD 23)

add_executable(bench main.cpp Graph.cpp RandomGraphGenerator.cpp bedrock.cpp)