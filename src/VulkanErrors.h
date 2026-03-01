#pragma once

#include <vulkan/vulkan.h>

#define VK_CHECK(x) vkCheck((x), __FILE__, __LINE__)

void vkCheck(VkResult result, const char* file, int line);
