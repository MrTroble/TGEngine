cmake_minimum_required (VERSION 3.15)

project ("TGEngine" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(TGEngine)
add_library(tc::TGEngine ALIAS TGEngine)