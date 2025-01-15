cmake_minimum_required(VERSION 3.5.0)
project(log_demo VERSION 0.1.0 LANGUAGES C CXX)
add_compile_options(-std=c++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(log_demo main.cc)

