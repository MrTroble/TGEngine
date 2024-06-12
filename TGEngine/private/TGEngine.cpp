#include "../public/TGEngine.hpp"

#include "../public/Util.hpp"
#include "../public/graphics/GUIModule.hpp"
#include "../public/graphics/PerformanceTestAPI.hpp"
#include "../public/graphics/vulkan/VulkanGraphicsModule.hpp"
#include <fstream>

namespace tge::main {

	std::vector<Module*> modules;
	std::vector<Module*> lateModules;
	bool isRunning = false;
	bool isInitialized = false;

	graphics::APILayer* usedApiLayer = nullptr;
	graphics::GameGraphicsModule* gameModule = nullptr;
	graphics::WindowModule* winModule = nullptr;

	void fireRecreate() {
		for (Module* modul : modules) {
			modul->recreate();
		}
	}

	Error init(const graphics::FeatureSet& featureSet) {
		if (isInitialized) return error = Error::ALREADY_INITIALIZED;
		static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
		{
			const std::ofstream stream1("TGELog.txt");
			const std::ofstream stream2("TGEVerboseLog.txt");
		}
		plog::init<1>(plog::debug, "TGELog.txt").addAppender(&consoleAppender);
		plog::init(plog::verbose, "TGEVerboseLog.txt")
			.addAppender(plog::get<1>());
		winModule = new graphics::WindowModule();
		modules.push_back(winModule);
		usedApiLayer = new graphics::PerformanceMessuringAPILayer(
			graphics::getNewVulkanModule());
		modules.push_back(usedApiLayer);
		gameModule =
			new graphics::GameGraphicsModule(usedApiLayer, winModule, featureSet);
		usedApiLayer->setGameGraphicsModule(gameModule);
		modules.push_back(gameModule);

		for (const auto m : lateModules) {
			modules.push_back(m);
		}
		lateModules.clear();

		for (auto mod : modules) {
			error = mod->init();
			if (error != Error::NONE) return error;
		}
		isInitialized = true;
		return error = Error::NONE;
	}

	Error start() {
		if (!isInitialized) return error = Error::NOT_INITIALIZED;
		if (isRunning) return error = Error::ALREADY_RUNNING;

		using namespace std::chrono;
		auto startpoint = steady_clock::now();
		std::array<double, 10> deltaTimes{ 0 };
		size_t currentIndex = 0;
		isRunning = true;
		for (;;) {
			if (util::exitRequest || winModule->closeRequest) break;
			double average = 0;
			for (const auto time : deltaTimes)
			{
				average += time;
			}
			average /= deltaTimes.size();
			for (auto mod : modules) mod->tick(average);

			auto endpoint = steady_clock::now();
			const std::chrono::duration<double> newDeltaTime = endpoint - startpoint;
			deltaTimes[currentIndex] = newDeltaTime.count();
			currentIndex = (currentIndex + 1) % deltaTimes.size();
			startpoint = endpoint;
		}
		for (auto bItr = modules.rbegin(); bItr < modules.rend(); bItr++) {
			(*bItr)->destroy();
			delete* bItr;
		}
		modules.clear();
		usedApiLayer = nullptr;
		gameModule = nullptr;
		winModule = nullptr;
		isRunning = false;
		isInitialized = false;
		util::exitRequest = false;
		return error = Error::NONE;
	}

	Error lastError() { return error; }

	graphics::APILayer* getAPILayer() { return usedApiLayer; }

	graphics::GameGraphicsModule* getGameGraphicsModule() { return gameModule; }

	void* getMainWindowHandle() { return winModule->hWnd; }

}  // namespace tge::main