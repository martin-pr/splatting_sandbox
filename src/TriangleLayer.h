#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "Renderer.h"

class TriangleLayer {
 public:
  explicit TriangleLayer(const Renderer::Context& ctx);
  ~TriangleLayer();

  TriangleLayer(const TriangleLayer&) = delete;
  TriangleLayer& operator=(const TriangleLayer&) = delete;

  void Render(VkCommandBuffer cmd, VkExtent2D extent) const;

 private:
  void Destroy();

  VkDevice device_ = VK_NULL_HANDLE;
  VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};
