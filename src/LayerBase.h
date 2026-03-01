#pragma once

#include <vulkan/vulkan.h>

#include "Renderer.h"

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

  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};
