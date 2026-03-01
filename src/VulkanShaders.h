#pragma once

#include <filesystem>

#include "VulkanHandles.h"

class ShaderModule
    : public VulkanHandle<VkShaderModule, vkDestroyShaderModule> {
 public:
  ShaderModule(VkDevice device, const std::filesystem::path& path);
};
