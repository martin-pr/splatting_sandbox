#pragma once

#include <vulkan/vulkan.h>

#define VK_CHECK(x) VkCheck((x), __FILE__, __LINE__)

void VkCheck(VkResult result, const char* file, int line);
