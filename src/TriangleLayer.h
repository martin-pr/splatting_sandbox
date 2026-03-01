#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "LayerBase.h"

class TriangleLayer : public PipelineLayerBase {
 public:
  explicit TriangleLayer(const Renderer::Context& ctx);

  void render(VkCommandBuffer cmd) const;

 private:
  VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
};
