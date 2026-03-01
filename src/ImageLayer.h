#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <vector>

#include "LayerBase.h"

class ImageLayer : public PipelineLayerBase {
 public:
  ImageLayer(const Renderer::Context& ctx, const std::filesystem::path& imagePath);
  ~ImageLayer();

  void Render(VkCommandBuffer cmd, VkExtent2D extent) const;

 private:
  std::vector<uint8_t> LoadImagePixels(const std::filesystem::path& path);
  void UploadTexture(const std::vector<uint8_t>& pixels,
                     VkPhysicalDevice physicalDevice,
                     VkQueue queue,
                     uint32_t queueFamily);
  void CreateDescriptors();
  void CreatePipeline(VkFormat swapchainFormat);
  void Destroy();

  static uint32_t FindMemoryType(VkPhysicalDeviceMemoryProperties memProps,
                                 uint32_t typeBits,
                                 VkMemoryPropertyFlags required);

  int imageWidth_ = 0;
  int imageHeight_ = 0;
  VkImage texture_ = VK_NULL_HANDLE;
  VkDeviceMemory textureMemory_ = VK_NULL_HANDLE;
  VkImageView textureView_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;

  VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
};
