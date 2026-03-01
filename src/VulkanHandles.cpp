#include "VulkanHandles.h"

#include "VulkanErrors.h"

Buffer::Buffer(VkDevice device, const VkBufferCreateInfo& ci) {
  device_ = device;
  VK_CHECK(vkCreateBuffer(device_, &ci, nullptr, &handle_));
}

CommandPool::CommandPool(VkDevice device, const VkCommandPoolCreateInfo& ci) {
  device_ = device;
  VK_CHECK(vkCreateCommandPool(device_, &ci, nullptr, &handle_));
}

DescriptorPool::DescriptorPool(VkDevice device, uint32_t maxSets,
                               const VkDescriptorPoolSize& poolSize) {
  device_ = device;
  const VkDescriptorPoolCreateInfo ci{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = maxSets,
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize,
  };
  VK_CHECK(vkCreateDescriptorPool(device_, &ci, nullptr, &handle_));
}

DescriptorSetLayout::DescriptorSetLayout(
    VkDevice device, const VkDescriptorSetLayoutBinding& binding) {
  device_ = device;
  const VkDescriptorSetLayoutCreateInfo ci{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &binding,
  };
  VK_CHECK(vkCreateDescriptorSetLayout(device_, &ci, nullptr, &handle_));
}

DeviceMemory::DeviceMemory(VkDevice device, const VkMemoryAllocateInfo& ai) {
  device_ = device;
  VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &handle_));
}

Image::Image(VkDevice device, const VkImageCreateInfo& ci) {
  device_ = device;
  VK_CHECK(vkCreateImage(device_, &ci, nullptr, &handle_));
}

ImageView::ImageView(VkDevice device, const VkImageViewCreateInfo& ci) {
  device_ = device;
  VK_CHECK(vkCreateImageView(device_, &ci, nullptr, &handle_));
}

Sampler::Sampler(VkDevice device, const VkSamplerCreateInfo& ci) {
  device_ = device;
  VK_CHECK(vkCreateSampler(device_, &ci, nullptr, &handle_));
}

PipelineLayout::PipelineLayout(VkDevice device,
                               const VkPipelineLayoutCreateInfo& ci) {
  device_ = device;
  VK_CHECK(vkCreatePipelineLayout(device_, &ci, nullptr, &handle_));
}

Pipeline::Pipeline(VkDevice device, const VkGraphicsPipelineCreateInfo& ci) {
  device_ = device;
  VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &ci, nullptr,
                                     &handle_));
}
