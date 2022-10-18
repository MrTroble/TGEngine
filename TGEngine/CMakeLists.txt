add_compile_definitions(ENGINE_NAME="TGEngine" ENGINE_VERSION=8388608 APPLICATION_NAME="Langrange Viewer" DEBUG)

if(WIN32)  
add_compile_options("/D_ITERATOR_DEBUG_LEVEL=0" "/MD" "/EHa")
add_link_options("/ignore:4099")
else()
include(FindX11)
add_compile_options("-Wno-deprecated-volatile")
endif()

include(FetchContent)
FETCHCONTENT_DECLARE(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
message("Downloading vulkansdk")
FETCHCONTENT_DECLARE(
	vulkansdk
	URL https://seafile.media-dienste.de/f/85da9d3e98b347a490f6/?dl=1

	BUILD_COMMAND ""
)

FETCHCONTENT_DECLARE(
	glm
	GIT_REPOSITORY https://github.com/g-truc/glm/
	GIT_TAG 0.9.9.8
)

FETCHCONTENT_DECLARE(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui
	GIT_TAG v1.88
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FETCHCONTENT_MAKEAVAILABLE(googletest)
FETCHCONTENT_MAKEAVAILABLE(vulkansdk)
FETCHCONTENT_MAKEAVAILABLE(imgui)
FETCHCONTENT_MAKEAVAILABLE(glm)

set(ENGINE_DATA   
	"public/TGEngine.hpp"
	"public/graphics/vulkan/VulkanGraphicsModule.hpp"
	"public/Module.hpp"
	"public/Error.hpp"
	"public/graphics/GameGraphicsModule.hpp"
	"public/Util.hpp"
	"private/TGEngine.cpp"
	"private/Error.cpp"
    "private/graphics/Vulkan/VulkanGraphicsModule.cpp"     
    "private/graphics/GameGraphicsModule.cpp"
    "private/Util.cpp"
	"private/graphics/WindowModule.cpp" 
	"private/graphics/Vulkan/VulkanShaderModule.cpp" 
	"public/graphics/vulkan/VulkanShaderModule.hpp"
	"public/graphics/Material.hpp" 
	"public/graphics/APILayer.hpp" 
	"private/graphics/Material.cpp"  
	"public/IO/IOModule.hpp" 
	"private/IO/IOModule.cpp"
    "public/graphics/vulkan/VulkanShaderPipe.hpp" 
	"public/graphics/GameShaderModule.hpp" 
	"private/graphics/GUIModule.cpp" 
	"public/graphics/GUIModule.hpp" 
	"public/graphics/vulkan/VulkanModuleDef.hpp"
	"${imgui_SOURCE_DIR}/imconfig.h"
	"${imgui_SOURCE_DIR}/imgui.cpp"
	"${imgui_SOURCE_DIR}/imgui.h"
	"${imgui_SOURCE_DIR}/imgui_demo.cpp"
	"${imgui_SOURCE_DIR}/imgui_draw.cpp"
	"${imgui_SOURCE_DIR}/imgui_internal.h"
	"${imgui_SOURCE_DIR}/imgui_tables.cpp"
	"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
	"${imgui_SOURCE_DIR}/imstb_rectpack.h"
	"${imgui_SOURCE_DIR}/imstb_textedit.h"
	"${imgui_SOURCE_DIR}/imstb_truetype.h"
	"${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.h"
)

add_library (TGEngine STATIC ${ENGINE_DATA})
target_link_libraries(TGEngine PUBLIC glslang shaderc_combined SPIRV glm)
if(WIN32)
target_link_libraries(TGEngine PUBLIC vulkan-1)
else()
target_link_libraries(TGEngine PUBLIC glslang-default-resource-limits vulkan m ${X11_X11_LIB})
endif()

if(WIN32)
target_include_directories(TGEngine PUBLIC "${vulkansdk_SOURCE_DIR}/Include" "${imgui_SOURCE_DIR}")
target_link_directories(TGEngine PUBLIC "${vulkansdk_SOURCE_DIR}/Lib")
list(APPEND ENGINE_DATA
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
	"${imgui_SOURCE_DIR}/backends/imgui_impl_win32.h"
)
else()
list(APPEND ENGINE_DATA
    "private/imgui/imgui_impl_x11.cpp"
	"public/imgui/imgui_impl_x11.h"
)
target_include_directories(TGEngine PUBLIC "usr/include" "${imgui_SOURCE_DIR}")
endif()
install(DIRECTORY "assets" DESTINATION $<TARGET_FILE_DIR:TGEngine>)
install(DIRECTORY "assets" DESTINATION $<TARGET_FILE_DIR:TGEngine>/..)
install(DIRECTORY "assets" DESTINATION $<TARGET_FILE_DIR:TGEngine>/../..)