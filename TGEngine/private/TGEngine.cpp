#include "../public/TGEngine.hpp"
#include "../public/gamecontent/Light.hpp"
#include "../public/pipeline/window/Window.hpp"
#include "../public/pipeline/buffer/UniformBuffer.hpp"
#include "../public/io/Resource.hpp"
#include "../public/resources/ShaderData.hpp"
#include "../public/io/Font.hpp"

using namespace std;
using namespace tge::tex;
using namespace tge::gmc;
using namespace tge::pip;
using namespace tge::buf;
using namespace tge::win;
using namespace tge::io;

void initEngine() {
	tge::nio::initFileSystem();
#ifndef TGE_NO_PROPERTY_FILE
	tgeproperties = tge::pro::readProperties("Properties.xml");
#endif // !TGE_NO_PROPERTY_FILE

	createWindowClass();
	createWindow();
	createInstance();
	createWindowSurfaces();
	createDevice();
	prePipeline();
	initShader();
	initShaderPipes();

	createDepthTest();
	createColorResouce();
	createRenderpass();
	initDescriptors();

	initPipelines();

	initCameras();

	initUniformBuffers();

	createCommandBuffer();

	createSwapchain();
	createFramebuffer();
}

void startTGEngine() {
	fillCommandBuffer();
	createMutex();

	clock_t last_time = clock();

	uint32_t counter = 0;

	while (true) {
		pollEvents();
		if (isCloseRequested) {
			break;
		}
		if (isMinimized) {
			continue;
		}
		startdraw();

		clock_t current_time = clock();
		clock_t delta = current_time - last_time;

		if (delta >= (CLOCKS_PER_SEC / 200)) {
			last_time = current_time;
			
			tge::win::mouseHomogeneousX = tge::win::mouseX * 2 / (float)tge::win::mainWindowWidth - 1.0f;
			tge::win::mouseHomogeneousY = tge::win::mouseY * 2 / (float)tge::win::mainWindowHeight - 1.0f;

			Input input = {};
			if (1 & states) {
				input.inputY = 0.004f;
			}
			if (2 & states) {
				input.inputY = -0.004f;
			}
			if (4 & states) {
				input.inputX = 0.004f;
			}
			if (8 & states) {
				input.inputX = -0.004f;
			}
			playercontroller(input);

			while (!executionQueue.empty()) {
				executionQueue.back()();
				executionQueue.pop_back();
			}
		}

		submit();
		present();

		// TESTING
		if (counter > 5) {
			//	exit(0);
		}
		counter++;
	}

	destroyCommandBuffer();
	destroyStagingBuffer();
	destroyFrameBuffer();
	destroyMutex();
	destroySwapchain();
	destroyRenderPass();
	destroyDepthTest();
	destroyColorResouce();
	destroyDescriptor();
	destroyResource();
	destroyBuffers(buffers, UBO_COUNT);
	tge::fnt::destroyFontresources();
	destroyPipelines();
	destroyShader();
	destroyDevice();
	destroyWindows();
	destroyInstance();
}
