

add_subdirectory(glfw)

add_library(imgui STATIC imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp 
	imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_vulkan.cpp 
	imgui/backends/imgui_impl_glfw.cpp)
target_include_directories(imgui PUBLIC imgui glfw)
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)

#add_subdirectory(glslang)