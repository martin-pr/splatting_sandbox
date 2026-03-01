#pragma once

#include <vulkan/vulkan.h>

template <typename Handle, auto DestroyFn>
class VulkanHandle {
 public:
  VulkanHandle() = default;
  ~VulkanHandle() {
    if (handle_ != VK_NULL_HANDLE)
      DestroyFn(device_, handle_, nullptr);
  }

  VulkanHandle(const VulkanHandle&) = delete;
  VulkanHandle& operator=(const VulkanHandle&) = delete;

  VulkanHandle(VkDevice device, Handle handle) noexcept
      : device_(device), handle_(handle) {}

  VulkanHandle(VulkanHandle&& other) noexcept
      : device_(other.device_), handle_(other.handle_) {
    other.handle_ = VK_NULL_HANDLE;
  }

  VulkanHandle& operator=(VulkanHandle&& other) noexcept {
    if (this != &other) {
      if (handle_ != VK_NULL_HANDLE)
        DestroyFn(device_, handle_, nullptr);
      device_ = other.device_;
      handle_ = other.handle_;
      other.handle_ = VK_NULL_HANDLE;
    }
    return *this;
  }

  Handle get() const { return handle_; }

 protected:
  VkDevice device_ = VK_NULL_HANDLE;
  Handle handle_ = VK_NULL_HANDLE;
};

using Buffer = VulkanHandle<VkBuffer, vkDestroyBuffer>;
using CommandPool = VulkanHandle<VkCommandPool, vkDestroyCommandPool>;
using DescriptorPool = VulkanHandle<VkDescriptorPool, vkDestroyDescriptorPool>;
using DescriptorSetLayout =
    VulkanHandle<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>;
using DeviceMemory = VulkanHandle<VkDeviceMemory, vkFreeMemory>;
using Image = VulkanHandle<VkImage, vkDestroyImage>;
using ImageView = VulkanHandle<VkImageView, vkDestroyImageView>;
using Sampler = VulkanHandle<VkSampler, vkDestroySampler>;

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
