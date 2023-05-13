#include <chrono>
#include <format>
#include "APILayer.hpp"

struct PrintableCounter {
  std::string name;
  double time = 0;
  size_t hitCount = 0;
  std::mutex mutex;

  std::string fullDebug() {
    std::lock_guard lg(mutex);
    if (hitCount == 0) return "";
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

  main::Error init() override {
    api->setGameGraphicsModule(this->getGraphicsModule());
    return api->init();
  }

  void tick(double deltatime) override { api->tick(deltatime); }

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

  void destroy() override {
    api->destroy();
    print();
  }

  void recreate() override {
    api->recreate();
    print();
  }

  [[nodiscard]] virtual std::vector<PipelineHolder> pushMaterials(
      const size_t materialcount, const Material* materials,
      const size_t offset = INVALID_SIZE_T) override {
    TimingAdder adder(materialCounter);
    const auto rtc = api->pushMaterials(materialcount, materials, offset);
    return rtc;
  };

  [[nodiscard]] virtual std::vector<TDataHolder> pushData(
      const size_t dataCount, const BufferInfo* bufferInfo) override {
    TimingAdder adder(dataCounter);
    const auto rtc = api->pushData(dataCount, bufferInfo);
    return rtc;
  }

  virtual void changeData(const size_t dataCount, const BufferChange* change) override {
    return api->changeData(dataCount, change);
  }

  virtual void removeRender(const size_t renderInfoCount,
                            const TRenderHolder* renderIDs) override {
    return api->removeRender(renderInfoCount, renderIDs);
  }

  [[nodiscard]] virtual TRenderHolder pushRender(
      const size_t renderInfoCount, const RenderInfo* renderInfos,
      const size_t offset = 0) override {
    TimingAdder adder(renderCounter);
    return api->pushRender(renderInfoCount, renderInfos);
  }

  [[nodiscard]] virtual TSamplerHolder pushSampler(
      const SamplerInfo& sampler) override {
    return api->pushSampler(sampler);
  }

  [[nodiscard]] virtual std::vector<TTextureHolder> pushTexture(
      const size_t textureCount, const TextureInfo* textures) override {
    TimingAdder adder(textureCounter);
    return api->pushTexture(textureCount, textures);
  }

  [[nodiscard]] virtual size_t pushLights(const size_t lightCount,
                                       const Light* lights,
                                       const size_t offset = 0) override {
    return api->pushLights(lightCount, lights, offset);
  }

  [[nodiscard]] virtual size_t getAligned(
      const size_t buffer, const size_t toBeAligned) const override {
    return api->getAligned(buffer, toBeAligned);
  }

  [[nodiscard]] virtual size_t getAligned(const DataType type) const override {
    return api->getAligned(type);
  }

  [[nodiscard]] virtual glm::vec2 getRenderExtent() const override {
    return api->getRenderExtent();
  }

  [[nodiscard]] virtual std::vector<char> getImageData(
      const size_t imageId, CacheIndex* cache = nullptr) override {
    return api->getImageData(imageId, cache);
  }

  virtual APILayer* backend() override { return api->backend(); }
};
}  // namespace tge::graphics