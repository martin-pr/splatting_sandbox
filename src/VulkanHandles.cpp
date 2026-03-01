#include "VulkanHandles.h"

#include "VulkanErrors.h"

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
