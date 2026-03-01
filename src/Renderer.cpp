#include "Renderer.h"

#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

#include "VulkanErrors.h"

Renderer::Renderer(SDL_Window* window) : window_(window) {
  try {
    InitInstanceAndSurface();
    InitDeviceAndSwapchain();
    InitCommandsAndSync();
  } catch (...) {
    Destroy();
    throw;
  }
}

Renderer::~Renderer() {
  Destroy();
}

void Renderer::Destroy() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);

    DestroySwapchainResources();
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;

    vkDestroySemaphore(device_, sync_.imageAvailable, nullptr);
    vkDestroyFence(device_, sync_.inFlight, nullptr);
    vkDestroyCommandPool(device_, commandPool_, nullptr);

    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }

  if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }

  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
  }
}

Renderer::Context Renderer::GetContext() const {
  return {
      instance_,
      physicalDevice_,
      device_,
      graphicsQueue_,
      graphicsQueueFamily_,
      swapchainFormat_,
      static_cast<uint32_t>(frames_.size()),
  };
}

VkExtent2D Renderer::GetSwapchainExtent() const {
  return swapchainExtent_;
}

bool Renderer::HasInstanceLayer(const char* layerName) {
  uint32_t layerCount = 0;
  if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS ||
      layerCount == 0)
    return false;

  std::vector<VkLayerProperties> layers(layerCount);
  if (vkEnumerateInstanceLayerProperties(&layerCount, layers.data()) !=
      VK_SUCCESS)
    return false;

  for (const auto& layer : layers) {
    if (std::strcmp(layer.layerName, layerName) == 0) return true;
  }
  return false;
}

VkPhysicalDevice Renderer::SelectPhysicalDevice(
    const std::vector<VkPhysicalDevice>& devices) {
  for (auto dev : devices) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(dev, &props);
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) return dev;
  }
  return devices[0];
}

Renderer::SwapchainConfig Renderer::SelectSwapchainConfig(
    SDL_Window* window, const VkSurfaceCapabilitiesKHR& caps,
    const std::vector<VkSurfaceFormatKHR>& formats,
    const std::vector<VkPresentModeKHR>& modes) {
  SwapchainConfig cfg{};
  cfg.format = formats[0];
  for (const auto& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      cfg.format = f;
      break;
    }
  }

  cfg.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  for (auto mode : modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      cfg.presentMode = mode;
      break;
    }
  }

  if (caps.currentExtent.width != UINT32_MAX) {
    cfg.extent = caps.currentExtent;
  } else {
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    cfg.extent.width = static_cast<uint32_t>(w);
    cfg.extent.height = static_cast<uint32_t>(h);

    cfg.extent.width =
        std::min(std::max(cfg.extent.width, caps.minImageExtent.width),
                 caps.maxImageExtent.width);
    cfg.extent.height =
        std::min(std::max(cfg.extent.height, caps.minImageExtent.height),
                 caps.maxImageExtent.height);
  }

  cfg.imageCount = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && cfg.imageCount > caps.maxImageCount)
    cfg.imageCount = std::min(cfg.imageCount, caps.maxImageCount);

  return cfg;
}

void Renderer::InitInstanceAndSurface() {
  uint32_t extCount = 0;
  const char* const* sdlExtensions =
      SDL_Vulkan_GetInstanceExtensions(&extCount);
  if (!sdlExtensions || extCount == 0)
    throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");

  std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extCount);
  std::vector<const char*> validationLayers;
#ifndef NDEBUG
  if (HasInstanceLayer("VK_LAYER_KHRONOS_validation"))
    validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  const VkApplicationInfo appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "splatting_sandbox",
      .apiVersion = VK_API_VERSION_1_3,
  };

  const VkInstanceCreateInfo ici{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
      .ppEnabledLayerNames = validationLayers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
  };

  if (vkCreateInstance(&ici, nullptr, &instance_) != VK_SUCCESS)
    throw std::runtime_error("vkCreateInstance failed");

  if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_))
    throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
}

void Renderer::InitDeviceAndSwapchain() {
  uint32_t physCount = 0;
  if (vkEnumeratePhysicalDevices(instance_, &physCount, nullptr) !=
          VK_SUCCESS ||
      physCount == 0)
    throw std::runtime_error("No Vulkan physical devices found");

  std::vector<VkPhysicalDevice> phys(physCount);
  VK_CHECK(vkEnumeratePhysicalDevices(instance_, &physCount, phys.data()));
  physicalDevice_ = SelectPhysicalDevice(phys);

  uint32_t queueCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueCount,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queues(queueCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueCount,
                                           queues.data());

  for (uint32_t i = 0; i < queueCount; ++i) {
    VkBool32 presentSupport = VK_FALSE;
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface_,
                                                  &presentSupport));
    if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
      graphicsQueueFamily_ = i;
      break;
    }
  }

  if (graphicsQueueFamily_ == UINT32_MAX)
    throw std::runtime_error(
        "No queue family supports both graphics and present");

  const float priority = 1.0f;
  const VkDeviceQueueCreateInfo qci{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = graphicsQueueFamily_,
      .queueCount = 1,
      .pQueuePriorities = &priority,
  };

  const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  const VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
      .dynamicRendering = VK_TRUE,
  };

  const VkDeviceCreateInfo dci{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &dynamicRenderingFeature,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &qci,
      .enabledExtensionCount = 1,
      .ppEnabledExtensionNames = deviceExtensions,
  };

  VK_CHECK(vkCreateDevice(physicalDevice_, &dci, nullptr, &device_));
  vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);

  CreateSwapchain(VK_NULL_HANDLE);
}

void Renderer::CreateSwapchain(VkSwapchainKHR oldSwapchain) {
  VkSurfaceCapabilitiesKHR caps{};
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_,
                                                     &caps));

  uint32_t formatCount = 0;
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_,
                                                &formatCount, nullptr));
  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_,
                                                &formatCount, formats.data()));

  uint32_t modeCount = 0;
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_,
                                                     &modeCount, nullptr));
  std::vector<VkPresentModeKHR> modes(modeCount);
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_,
                                                     &modeCount, modes.data()));

  SwapchainConfig sc = SelectSwapchainConfig(window_, caps, formats, modes);
  swapchainFormat_ = sc.format.format;
  swapchainExtent_ = sc.extent;

  const VkSwapchainCreateInfoKHR sci{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface_,
      .minImageCount = sc.imageCount,
      .imageFormat = sc.format.format,
      .imageColorSpace = sc.format.colorSpace,
      .imageExtent = sc.extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = caps.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = sc.presentMode,
      .clipped = VK_TRUE,
      .oldSwapchain = oldSwapchain,
  };

  VK_CHECK(vkCreateSwapchainKHR(device_, &sci, nullptr, &swapchain_));

  if (oldSwapchain != VK_NULL_HANDLE)
    vkDestroySwapchainKHR(device_, oldSwapchain, nullptr);

  uint32_t swapImageCount = 0;
  VK_CHECK(
      vkGetSwapchainImagesKHR(device_, swapchain_, &swapImageCount, nullptr));
  std::vector<VkImage> swapchainImages(swapImageCount);
  VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &swapImageCount,
                                   swapchainImages.data()));

  frames_.resize(swapImageCount);
  for (uint32_t i = 0; i < swapImageCount; ++i) {
    frames_[i].image = swapchainImages[i];
    frames_[i].initialized = false;

    const VkImageViewCreateInfo ivci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = frames_[i].image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchainFormat_,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    VK_CHECK(vkCreateImageView(device_, &ivci, nullptr, &frames_[i].view));
  }
}

void Renderer::AllocateFrameCommandsAndSync() {
  std::vector<VkCommandBuffer> commandBuffers(frames_.size(), VK_NULL_HANDLE);
  const VkCommandBufferAllocateInfo cbai{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
  };
  VK_CHECK(vkAllocateCommandBuffers(device_, &cbai, commandBuffers.data()));
  for (size_t i = 0; i < frames_.size(); ++i)
    frames_[i].commandBuffer = commandBuffers[i];

  VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  for (auto& frame : frames_)
    VK_CHECK(
        vkCreateSemaphore(device_, &semInfo, nullptr, &frame.renderFinished));
}

void Renderer::DestroySwapchainResources() {
  if (device_ == VK_NULL_HANDLE) return;

  std::vector<VkCommandBuffer> cmds;
  for (auto& frame : frames_) {
    if (frame.commandBuffer != VK_NULL_HANDLE)
      cmds.push_back(frame.commandBuffer);
    if (frame.renderFinished != VK_NULL_HANDLE)
      vkDestroySemaphore(device_, frame.renderFinished, nullptr);
    if (frame.view != VK_NULL_HANDLE)
      vkDestroyImageView(device_, frame.view, nullptr);
  }
  if (commandPool_ != VK_NULL_HANDLE && !cmds.empty())
    vkFreeCommandBuffers(device_, commandPool_,
                         static_cast<uint32_t>(cmds.size()), cmds.data());
  frames_.clear();
}

void Renderer::RecreateSwapchain() {
  int w = 0, h = 0;
  SDL_GetWindowSizeInPixels(window_, &w, &h);
  if (w == 0 || h == 0) return;  // window is minimized; skip

  vkDeviceWaitIdle(device_);

  DestroySwapchainResources();

  VkSwapchainKHR oldSwapchain = swapchain_;
  swapchain_ = VK_NULL_HANDLE;

  CreateSwapchain(oldSwapchain);
  AllocateFrameCommandsAndSync();
}

void Renderer::InitCommandsAndSync() {
  const VkCommandPoolCreateInfo cpci{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphicsQueueFamily_,
  };
  VK_CHECK(vkCreateCommandPool(device_, &cpci, nullptr, &commandPool_));

  const VkSemaphoreCreateInfo semInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  const VkFenceCreateInfo fenceInfo{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  VK_CHECK(
      vkCreateSemaphore(device_, &semInfo, nullptr, &sync_.imageAvailable));
  VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &sync_.inFlight));

  AllocateFrameCommandsAndSync();
}

void Renderer::RenderFrame(const std::function<void(VkCommandBuffer)>& drawFn) {
  int w = 0, h = 0;
  SDL_GetWindowSizeInPixels(window_, &w, &h);
  if (w == 0 || h == 0) return;  // window is minimized; skip

  VK_CHECK(vkWaitForFences(device_, 1, &sync_.inFlight, VK_TRUE, UINT64_MAX));

  uint32_t imageIndex = 0;
  VkResult acquire =
      vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                            sync_.imageAvailable, VK_NULL_HANDLE, &imageIndex);

  if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapchain();
    return;
  }
  if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) VK_CHECK(acquire);

  // Reset fence only after we know we'll submit work this frame
  VK_CHECK(vkResetFences(device_, 1, &sync_.inFlight));

  auto& frame = frames_[imageIndex];
  VkCommandBuffer cmd = frame.commandBuffer;
  VkSemaphore renderFinished = frame.renderFinished;
  VK_CHECK(vkResetCommandBuffer(cmd, 0));

  VkCommandBufferBeginInfo beginInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

  const VkImageMemoryBarrier toColor{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .oldLayout = frame.initialized ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                     : VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = frame.image,
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .levelCount = 1,
          .layerCount = 1,
      },
  };
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &toColor);
  frame.initialized = true;

  const VkRenderingAttachmentInfo colorAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = frame.view,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {.color = {{0.08f, 0.09f, 0.11f, 1.0f}}},
  };

  const VkRenderingInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.extent = swapchainExtent_},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
  };

  vkCmdBeginRendering(cmd, &renderingInfo);

  const VkViewport viewport{
      .width = static_cast<float>(swapchainExtent_.width),
      .height = static_cast<float>(swapchainExtent_.height),
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  const VkRect2D scissor{.extent = swapchainExtent_};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  if (drawFn) drawFn(cmd);

  vkCmdEndRendering(cmd);

  const VkImageMemoryBarrier toPresent{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = frame.image,
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .levelCount = 1,
          .layerCount = 1,
      },
  };
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &toPresent);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  const VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &sync_.imageAvailable,
      .pWaitDstStageMask = waitStages,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &renderFinished,
  };
  VK_CHECK(vkQueueSubmit(graphicsQueue_, 1, &submitInfo, sync_.inFlight));

  const VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &renderFinished,
      .swapchainCount = 1,
      .pSwapchains = &swapchain_,
      .pImageIndices = &imageIndex,
  };

  VkResult present = vkQueuePresentKHR(graphicsQueue_, &presentInfo);
  if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR) {
    RecreateSwapchain();
    return;
  }
  if (present != VK_SUCCESS) VK_CHECK(present);
}
