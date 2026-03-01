#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "LayerBase.h"

class TriangleLayer : public PipelineLayerBase {
 public:
  explicit TriangleLayer(const Renderer::Context& ctx);
  ~TriangleLayer();

  void Render(VkCommandBuffer cmd) const;

 private:
  void Destroy();

  VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
};
