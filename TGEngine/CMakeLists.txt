cmake_minimum_required (VERSION 3.24)
set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(ENGINE_NAME="TGEngine" ENGINE_VERSION=8388608)

if(WIN32)  
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_link_options("/ignore:4099")
else()
include(FindX11)
add_compile_options("-Wno-deprecated-volatile")
endif()

include(FetchContent)
FETCHCONTENT_DECLARE(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FETCHCONTENT_MAKEAVAILABLE(googletest)

set(ENGINE_DATA   
	"private/TGEngine.cpp"
	"private/Error.cpp"
    "private/graphics/Vulkan/VulkanGraphicsModule.cpp"     
    "private/graphics/GameGraphicsModule.cpp"
    "private/Util.cpp"
	"private/graphics/WindowModule.cpp" 
	"private/graphics/Vulkan/VulkanShaderModule.cpp" 
	"private/IO/IOModule.cpp"
	"private/graphics/GUIModule.cpp" 
)

add_library(TGEngine STATIC ${ENGINE_DATA})

message(${Vulkan_glslang_LIBRARY})
target_include_directories(TGEngine PUBLIC ../submodules/glfw ../submodules/imgui ../submodules/glm)
target_link_libraries(TGEngine PUBLIC Vulkan::Vulkan glfw plog::plog imgui Vulkan::glslang Vulkan::shaderc_combined Vulkan::SPIRV-Tools)

find_package(plog CONFIG REQUIRED)
target_link_libraries(TGEngine PRIVATE )

enable_testing()

add_executable(TGEngineTests "test/TGTests.cpp")
target_link_libraries(TGEngineTests PRIVATE plog::plog GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(TGEngineTests)

install(DIRECTORY "assets" DESTINATION $<TARGET_FILE_DIR:TGEngine>)
install(DIRECTORY "assets" DESTINATION $<TARGET_FILE_DIR:TGEngine>/..)
install(DIRECTORY "assets" DESTINATION $<TARGET_FILE_DIR:TGEngine>/../..)
