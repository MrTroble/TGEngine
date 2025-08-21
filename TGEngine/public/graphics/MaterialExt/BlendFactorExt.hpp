#pragma once

#include <vulkan/vulkan.hpp>

namespace tge::graphics {
	
	struct BlendFactorExt {
		vk::BlendFactor srcColorFactor = vk::BlendFactor::eOne;
		vk::BlendFactor dstColorFactor = vk::BlendFactor::eZero;
		vk::BlendFactor srcAlphaFactor = vk::BlendFactor::eZero;
		vk::BlendFactor dstAlphaFactor = vk::BlendFactor::eZero;
	};
}