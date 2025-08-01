#pragma once

#include <vulkan/vulkan.hpp>

namespace tge::graphics {
	
	struct BlendFactorExt {
		vk::BlendFactor srcColorFactor = vk::BlendFactor::eOne;
		vk::BlendFactor dstColorFactor = vk::BlendFactor::eZero;
	};

}