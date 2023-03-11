#include <chrono>

#include "APILayer.hpp"

struct PrintableCounter {
  std::string name;
  double time = 0;
  size_t hitCount = 0;
  std::mutex mutex;

  std::string fullDebug() {
    std::lock_guard lg(mutex);
    return std::format("[{}: overallTime={}, hitCount={}, averageTime={}]",
                       name, time, hitCount, time / (double)hitCount);
  }

  void print() {
    const auto string = fullDebug();
    printf("%s\n", string.c_str());
  }
};

class TimingAdder {
  PrintableCounter& counter;
  std::chrono::steady_clock::time_point clock;

 public:
  TimingAdder(PrintableCounter& counter)
      : counter(counter), clock(std::chrono::steady_clock::now()) {}

  ~TimingAdder() {
    using namespace std::chrono;
    std::lock_guard lg(counter.mutex);
    counter.hitCount++;
    counter.time += duration_cast<duration<double>>(
                        std::chrono::steady_clock::now() - clock)
                        .count();
  }
};

namespace tge::graphics {
class PerformanceMessuringAPILayer : public APILayer {
 public:
  APILayer* api;
  PrintableCounter materialCounter{"Materials"};
  PrintableCounter dataCounter{"Data"};
  PrintableCounter renderCounter{"Render"};
  PrintableCounter textureCounter{"Texture"};

  PerformanceMessuringAPILayer(APILayer* api)
      : APILayer(api->getShaderAPI()), api(api) {}

  ~PerformanceMessuringAPILayer() { delete api; }

  main::Error init() {
    api->setGameGraphicsModule(this->getGraphicsModule());
    return api->init();
  }

  void tick(double deltatime) { api->tick(deltatime); }

  std::string getDebug() {
    return materialCounter.fullDebug() + "\n" + dataCounter.fullDebug() + "\n" +
           renderCounter.fullDebug() + "\n" + textureCounter.fullDebug() + "\n";
  }

  void print() {
    materialCounter.print();
    dataCounter.print();
    renderCounter.print();
    textureCounter.print();
  }

  void destroy() {
    api->destroy();
    print();
  }

  void recreate() {
    api->recreate();
    print();
  }

  _NODISCARD virtual void* loadShader(const MaterialType type) {
    return api->loadShader(type);
  }

  _NODISCARD virtual std::vector<PipelineHolder> pushMaterials(const size_t materialcount,
                                          const Material* materials,
                                          const size_t offset = SIZE_MAX) {
    TimingAdder adder(materialCounter);
    const auto rtc = api->pushMaterials(materialcount, materials, offset);
    return rtc;
  };

  _NODISCARD virtual size_t pushData(const size_t dataCount, void* data,
                                     const size_t* dataSizes,
                                     const DataType type) {
    TimingAdder adder(dataCounter);
    const auto rtc = api->pushData(dataCount, data, dataSizes, type);
    return rtc;
  }

  virtual void changeData(const size_t bufferIndex, const void* data,
                          const size_t dataSizes, const size_t offset = 0) {
    return api->changeData(bufferIndex, data, dataSizes, offset);
  }

  virtual size_t removeRender(const size_t renderInfoCount,
                              const size_t* renderIDs) {
    return api->removeRender(renderInfoCount, renderIDs);
  }

  virtual size_t pushRender(const size_t renderInfoCount,
                            const RenderInfo* renderInfos,
                            const size_t offset = 0) {
    TimingAdder adder(renderCounter);
    return api->pushRender(renderInfoCount, renderInfos);
  }

  _NODISCARD virtual size_t pushSampler(const SamplerInfo& sampler) {
    return api->pushSampler(sampler);
  }

  _NODISCARD virtual size_t pushTexture(const size_t textureCount,
                                        const TextureInfo* textures) {
    TimingAdder adder(textureCounter);
    return api->pushTexture(textureCount, textures);
  }

  _NODISCARD virtual size_t pushLights(const size_t lightCount,
                                       const Light* lights,
                                       const size_t offset = 0) {
    return api->pushLights(lightCount, lights, offset);
  }

  _NODISCARD virtual size_t getAligned(const size_t buffer,
                                       const size_t toBeAligned) const {
    return api->getAligned(buffer, toBeAligned);
  }

  _NODISCARD virtual size_t getAligned(const DataType type) const {
    return api->getAligned(type);
  }

  _NODISCARD virtual glm::vec2 getRenderExtent() const {
    return api->getRenderExtent();
  }

  _NODISCARD virtual std::vector<char> getImageData(
      const size_t imageId, CacheIndex* cache = nullptr) {
    return api->getImageData(imageId, cache);
  }

  virtual APILayer* backend() { return api->backend(); }
};
}  // namespace tge::graphics