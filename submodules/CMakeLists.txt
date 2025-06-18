set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_BUILD_EXAMPLES OFF)

set(BUILD_EXTERNAL OFF)
set(ALLOW_EXTERNAL_SPIRV_TOOLS ON)
set(ENABLE_OPT ON)

add_subdirectory(glfw)
add_subdirectory(plog)
add_subdirectory(SPIRV-Headers)
add_subdirectory(SPIRV-Tools)
add_subdirectory(glslang)

add_library(imgui STATIC imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp 
	imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_vulkan.cpp 
	imgui/backends/imgui_impl_glfw.cpp)
target_include_directories(imgui PUBLIC imgui glfw)
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)

#add_subdirectory(glslang)