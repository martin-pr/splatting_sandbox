#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <vector>

#include "LayerBase.h"

class ImageLayer : public PipelineLayerBase {
 public:
  ImageLayer(const Renderer::Context& ctx, const std::filesystem::path& imagePath);

  void render(VkCommandBuffer cmd, VkExtent2D extent) const;

 private:
  std::vector<uint8_t> loadImagePixels(const std::filesystem::path& path);
  void uploadTexture(const std::vector<uint8_t>& pixels,
                     VkPhysicalDevice physicalDevice,
                     VkQueue queue,
                     uint32_t queueFamily);
  void createDescriptors();
  void createPipeline(VkFormat swapchainFormat);

  static uint32_t findMemoryType(VkPhysicalDeviceMemoryProperties memProps,
                                 uint32_t typeBits,
                                 VkMemoryPropertyFlags required);

  int imageWidth_ = 0;
  int imageHeight_ = 0;
  Image texture_;
  DeviceMemory textureMemory_;
  ImageView textureView_;
  Sampler sampler_;

  DescriptorSetLayout descriptorSetLayout_;
  DescriptorPool descriptorPool_;
  VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
};
