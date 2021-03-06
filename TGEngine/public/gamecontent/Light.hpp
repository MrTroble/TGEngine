#pragma once

#include "../../public/Stdbase.hpp"
#include "../../public/pipeline/buffer/UniformBuffer.hpp"
#include "../../public/gamecontent/Actor.hpp"

namespace tge {
	namespace gmc {

		/*
		 * Maximum count of lights used in the scene
		 */
		extern uint32_t lightCount;

		/*
		 * Holds the information about lights
		 *
		 * <ul>
		 * <li><strong class='atr'>pos</strong> is the position in the coordinate system</li>
		 * <li><strong class='atr'>intensity</strong> is the intensity of the light</li>
		 * <li><strong class='atr'>id</strong> is the id in the light array</li></ul>
		 * <strong>This API is deprecated!</strong>
		 */
		class LightActor {

		private:
			glm::vec4 pos;
			glm::vec4 intensity;
			uint32_t  id;

		public:
			LightActor(uint32_t intensity, glm::vec3 color, glm::vec3 pos);

			void updateLight();
		};

		/* Holds references to the light actors
		 *
         * <strong>This API is deprecated!</strong>
		 */
		extern std::vector<LightActor*> lights;
	}
}
