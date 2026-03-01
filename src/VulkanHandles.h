#pragma once

#include <vulkan/vulkan.h>

template <typename Handle, auto DestroyFn>
class VulkanHandle {
 public:
  VulkanHandle() = default;
  ~VulkanHandle() {
    if (handle_ != VK_NULL_HANDLE) {
      DestroyFn(device_, handle_, nullptr);
    }
  }

  VulkanHandle(const VulkanHandle&) = delete;
  VulkanHandle& operator=(const VulkanHandle&) = delete;

  VulkanHandle(VulkanHandle&& other) noexcept
      : device_(other.device_), handle_(other.handle_) {
    other.handle_ = VK_NULL_HANDLE;
  }

  VulkanHandle& operator=(VulkanHandle&& other) noexcept {
    if (this != &other) {
      if (handle_ != VK_NULL_HANDLE) {
        DestroyFn(device_, handle_, nullptr);
      }
      device_ = other.device_;
      handle_ = other.handle_;
      other.handle_ = VK_NULL_HANDLE;
    }
    return *this;
  }

  [[nodiscard]] Handle get() const { return handle_; }

 protected:
  VulkanHandle(VkDevice device, Handle handle) noexcept
      : device_(device), handle_(handle) {}

  VkDevice device_ = VK_NULL_HANDLE;
  Handle handle_ = VK_NULL_HANDLE;
};

class Buffer : public VulkanHandle<VkBuffer, vkDestroyBuffer> {
 public:
  Buffer() = default;
  Buffer(VkDevice device, const VkBufferCreateInfo& ci);
};

class CommandPool : public VulkanHandle<VkCommandPool, vkDestroyCommandPool> {
 public:
  CommandPool() = default;
  CommandPool(VkDevice device, const VkCommandPoolCreateInfo& ci);
};

class DescriptorPool
    : public VulkanHandle<VkDescriptorPool, vkDestroyDescriptorPool> {
 public:
  DescriptorPool() = default;
  DescriptorPool(VkDevice device, uint32_t maxSets,
                 const VkDescriptorPoolSize& poolSize);
};

class DescriptorSetLayout
    : public VulkanHandle<VkDescriptorSetLayout,
                          vkDestroyDescriptorSetLayout> {
 public:
  DescriptorSetLayout() = default;
  DescriptorSetLayout(VkDevice device,
                      const VkDescriptorSetLayoutBinding& binding);
};

class DeviceMemory : public VulkanHandle<VkDeviceMemory, vkFreeMemory> {
 public:
  DeviceMemory() = default;
  DeviceMemory(VkDevice device, const VkMemoryAllocateInfo& ai);
};

class Image : public VulkanHandle<VkImage, vkDestroyImage> {
 public:
  Image() = default;
  Image(VkDevice device, const VkImageCreateInfo& ci);
};

class ImageView : public VulkanHandle<VkImageView, vkDestroyImageView> {
 public:
  ImageView() = default;
  ImageView(VkDevice device, const VkImageViewCreateInfo& ci);
};

class Sampler : public VulkanHandle<VkSampler, vkDestroySampler> {
 public:
  Sampler() = default;
  Sampler(VkDevice device, const VkSamplerCreateInfo& ci);
};

class PipelineLayout
    : public VulkanHandle<VkPipelineLayout, vkDestroyPipelineLayout> {
 public:
  PipelineLayout() = default;
  PipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo& ci);
};

class Pipeline : public VulkanHandle<VkPipeline, vkDestroyPipeline> {
 public:
  Pipeline() = default;
  Pipeline(VkDevice device, const VkGraphicsPipelineCreateInfo& ci);
};
