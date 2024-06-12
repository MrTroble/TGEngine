#include "GUIModule.hpp"
#include "headerlibs/json.hpp"
#include "vulkan/VulkanShaderModule.hpp"
#include <fstream>

class ShaderUI {
	size_t selected = 0;
	nlohmann::json jsonObj;

public:
	void renderGUI(tge::shader::ShaderAPI* shader) {
		if (shader == nullptr) return;
		std::vector<std::string> namesUsed;
		{
			tge::shader::VulkanShaderModule* shaderModule = (tge::shader::VulkanShaderModule*)shader;
			std::lock_guard guard(shaderModule->mutex);
			namesUsed.reserve(2 * shaderModule->shaderPipes.size());
			for (auto value : shaderModule->shaderPipes) {
				for (auto& name : value->shaderNames) {
					if (name.empty())
						continue;
					namesUsed.push_back(name);
				}
			}
		}
		if (namesUsed.empty())
			return;
		size_t container = 0;
		if (ImGui::Begin("ShaderUI")) {
			const auto combobox = selected >= namesUsed.size() ? "Unknown" : namesUsed[selected].c_str();
			if (ImGui::BeginCombo("Shader", combobox)) {
				for (auto& string : namesUsed) {
					container++;
					if (ImGui::Selectable(string.c_str())) {
						selected = container;
						std::ifstream stream(string);
						jsonObj << stream;
					}
				}
				ImGui::EndCombo();
			}
			if (namesUsed.size() > selected) {
				size_t index = 0;
				for (auto& jelement : jsonObj["codes"]) {
					const auto blockName = "Block" + std::to_string(index);
					auto& codeShader = jelement["code"];
					std::string text;
					for (auto& string : codeShader) {
						text += string.get<std::string>() + "\n";
					}
					if (text.size() < 400)
						text.resize(400);
					ImGui::InputTextMultiline(blockName.c_str(), text.data(), text.size());
					if (ImGui::TreeNode(blockName.c_str())) {
						auto& dependable = jelement["dependsOn"];
						std::string dependables;
						for (auto& string : dependable) {
							dependables += string.get<std::string>() + "; ";
						}
						if (dependables.size() < 400)
							dependables.resize(100);
						ImGui::InputText("Depends on", dependables.data(), dependables.size());
						ImGui::TreePop();
					}
					index++;
				}
			}
		}
		ImGui::End();
	}

};