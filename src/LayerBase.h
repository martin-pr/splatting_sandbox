#pragma once

#include <vulkan/vulkan.h>

#include "Renderer.h"
#include "VulkanHandles.h"

class LayerBase {
 public:
  LayerBase(const LayerBase&) = delete;
  LayerBase& operator=(const LayerBase&) = delete;

 protected:
  explicit LayerBase(const Renderer::Context& ctx) : device_(ctx.device) {}
  ~LayerBase() = default;

  VkDevice device_ = VK_NULL_HANDLE;
};

class PipelineLayerBase : public LayerBase {
 protected:
  explicit PipelineLayerBase(const Renderer::Context& ctx) : LayerBase(ctx) {}
  ~PipelineLayerBase() = default;

  PipelineLayout pipelineLayout_;
  Pipeline pipeline_;
};
