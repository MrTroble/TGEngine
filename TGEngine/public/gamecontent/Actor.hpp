#pragma once

#include <iostream>
#include "../../public/Error.hpp"
#include "../../public/util/VectorUtil.hpp"
#include "../../public/gamecontent/Material.hpp"

namespace tge::gmc {

	/*
     * This struct holds all the transform information
	 * used within a drawcall by this actor
     *
     * <ul>
     * <li><strong class='atr'>matrix</strong> is the transform information</li>
     * <li><strong class='atr'>animationIndex</strong> is the index of the material used to draw this mesh</li>
     * <li><strong class='atr'>transformIndex</strong> is the index of the object transform used to draw this mesh</li></ul>
     *
     * <h4>Valid usage</h4>
     * <ul>
     * <li><strong class='atr'>matrix/strong> must be a valid matrix</li>
     * <li><strong class='atr'>animationIndex</strong> must be a valid material id in the material list</li>
     * <li><strong class='atr'>transformIndex</strong> must be a valid transform id in the transform list</li></ul>
     */
	struct ActorTransform {
		glm::fmat4 matrix;
		uint32_t   animationIndex;
		uint32_t   transformIndex;
	};

	/*
     * This struct holds all the properties that
	 * do not directly influence the mesh itself
     *
     * <ul>
     * <li><strong class='atr'>transform</strong> is the transform information</li>
     * <li><strong class='atr'>material</strong> is the index of the material used to draw this mesh</li>
     * <li><strong class='atr'>layer</strong> is the layer the actor is drawn on (e.g. 0 for normal world)</li></ul>
     *
     * <h4>Valid usage</h4>
     * <ul>
     * <li><strong class='atr'>transform</strong> must be a valid ActorTransform struct</li>
     * <li><strong class='atr'>material</strong> must be a valid material id in the material list</li>
     * <li><strong class='atr'>layer</strong> must be a valid layer id, otherwise this actor will be ingnored</li></ul>
     */
	struct ActorProperties {
		ActorTransform transform;
		uint32_t       material;
		uint32_t       layer;
	};

	/*
     * This struct holds all the information
     * needed to draw an actor, such as offsets in the buffer
	 * and index count
	 *
     * <ul>
     * <li><strong class='atr'>indexDrawCount</strong> is the count of drawn indices</li>
     * <li><strong class='atr'>indexOffset</strong> is the first index to be drawn by this actor in the given map</li>
     * <li><strong class='atr'>vertexOffset</strong> is the first vertex the indices are describing</li>
     * <li><strong class='atr'>instanceID</strong> is the id in the actorInstanceDescriptor array</li></ul>
     *
     * <h4>Valid usage</h4>
     * <ul>
	 * <li><strong class='atr'>indexDrawCount</strong> needs to be greater then 3</li>
	 * <li><strong class='atr'>indexOffset</strong> needs to be smaller then the maximum index count of the map buffer</li>
	 * <li><strong class='atr'>vertexOffset</strong> needs to be smaller then the maximum vertex count + current index</li>
	 * <li><strong class='atr'>instanceID</strong> must be a valid id in the actorInstanceDescriptor array or UINT32_MAX</li></ul>
	 */
	struct ActorDescriptor {
		uint32_t indexDrawCount;
		uint32_t indexOffset;
		uint32_t vertexOffset;
		uint32_t instanceID;
	};

	/*
	 * This struct holds all the information
	 * needed to draw an instance of this actor
	 *
	 * <ul>
	 * <li><strong class='atr'>instanceCount</strong> is the count of instances to be drawn</li>
	 * <li><strong class='atr'>instanceOffset</strong> is the first instance to start with</li>
	 * </ul>
	 *
	 * <h4>Valid usage</h4>
	 * <ul>
	 * <li><strong class='atr'>instanceCount</strong> needs to be greater then 1</li>
	 * <li><strong class='atr'>instanceOffset</strong> needs to be smaller then the maximum instance count of the map buffer</li>
	 * </ul>
	 */
	struct ActorInstanceDescriptor {
		uint32_t instanceCount;
		uint32_t instanceOffset;
	};

	/*
	 * Holds the properties for each actor
	 */
	extern std::vector<ActorProperties> actorProperties;
	/*
     * Holds the description of an actor
     */
	extern std::vector<ActorDescriptor> actorDescriptor;
	/*
	 * Holds the instance rendering info of an actor
	 */
	extern std::vector<ActorInstanceDescriptor> actorInstanceDescriptor;

	/*
	 * Internal method which loads every actor
	 * into the according command buffer
	 */
	void loadToCommandBuffer(VkCommandBuffer pBuffer, uint8_t pLayerId);

}
