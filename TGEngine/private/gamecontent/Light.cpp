#include "../../public/gamecontent/Light.hpp"

namespace tge {
	namespace gmc {

		std::vector<LightActor*> lights;
		uint32_t lightCount = 0;

		LightActor::LightActor(uint32_t intensity, glm::vec3 color, glm::vec3 pos) : intensity(glm::vec4(color, intensity)), pos(glm::vec4(pos, 1)) {
			this->id = lightCount++;
		}

		void LightActor::updateLight() {
			// TODO fillUniformBuffer(&lightbuffer, this, 2 * sizeof(glm::vec4), this->id * 2 * sizeof(glm::vec4));
		}
	}
}