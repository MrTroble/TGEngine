cmake_minimum_required (VERSION 3.25)

project ("TGEngine" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

include(FindVulkan)
add_subdirectory(submodules)

add_subdirectory(TGEngine)
add_library(tc::TGEngine ALIAS TGEngine)