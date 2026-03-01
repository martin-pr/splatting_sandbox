#pragma once

#include <vulkan/vulkan.h>

#include "Renderer.h"
#include "VulkanHandles.h"

class LayerBase {
 public:
  LayerBase(const LayerBase&) = delete;
  LayerBase& operator=(const LayerBase&) = delete;
  LayerBase(LayerBase&&) = delete;
  LayerBase& operator=(LayerBase&&) = delete;

 protected:
  explicit LayerBase(const Renderer::Context& ctx) : device_(ctx.device) {}
  ~LayerBase() = default;

  VkDevice device_ = VK_NULL_HANDLE;
};

class PipelineLayerBase : public LayerBase {
 public:
  PipelineLayerBase(const PipelineLayerBase&) = delete;
  PipelineLayerBase& operator=(const PipelineLayerBase&) = delete;
  PipelineLayerBase(PipelineLayerBase&&) = delete;
  PipelineLayerBase& operator=(PipelineLayerBase&&) = delete;

 protected:
  explicit PipelineLayerBase(const Renderer::Context& ctx) : LayerBase(ctx) {}
  ~PipelineLayerBase() = default;

  PipelineLayout pipelineLayout_;
  Pipeline pipeline_;
};
