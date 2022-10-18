# Project-Lagrange

[![GitHub](https://img.shields.io/github/license/MrTroble/Project-Lagrange?style=for-the-badge)](https://github.com/MrTroble/Project-Lagrange/blob/master/LICENSE)
![Cpp Version 20](https://img.shields.io/badge/C++%20Version-20-red?style=for-the-badge&logo=cplusplus)
![Compiler support](https://img.shields.io/badge/Compiler-clang%2014%20|%20msvc%2019%20|%20gcc%2010-blue?style=for-the-badge&logo=cplusplus)
[![GitHub Workflow Status](https://img.shields.io/github/workflow/status/MrTroble/Project-Lagrange/CMake?style=for-the-badge)](https://github.com/MrTroble/Project-Lagrange/actions)


## Build
### Build instructions

#### Prerequirements

> Note: All requirements are installed by cmake on windows

On Linux you need the Vulkan SDK package [https://vulkan.lunarg.com/sdk/home#linux](https://vulkan.lunarg.com/sdk/home#linux)

#### Cmake

* cmake ./TGEngine -B ./build -DCMAKE_BUILD_TYPE=Debug

* cmake --build ./build --config Debug

> Note: Not all assets are currently copied to the output folder correctly, you can manually copy TGEngine/test/assets and TGEngine/application/assets to the output folder

## Libraries

* [LunarG's Vulkan-SDK](https://vulkan.lunarg.com/sdk/home).
* STB
* tinygltf
* json.hpp
* Shaderpermute
* glm
* gtest
