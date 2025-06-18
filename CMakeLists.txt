cmake_minimum_required (VERSION 3.24)

project ("TGEngine" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan 1.4 REQUIRED)
add_subdirectory(submodules)

add_subdirectory(TGEngine)
add_library(tc::TGEngine ALIAS TGEngine)