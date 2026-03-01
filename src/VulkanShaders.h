#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>

class ShaderModule {
 public:
  ShaderModule(VkDevice device, const std::filesystem::path& path);
  ~ShaderModule();

  ShaderModule(const ShaderModule&) = delete;
  ShaderModule& operator=(const ShaderModule&) = delete;

  VkShaderModule get() const { return module_; }

 private:
  VkDevice device_;
  VkShaderModule module_ = VK_NULL_HANDLE;
};
