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

  VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  appInfo.pApplicationName = "splatting_sandbox";
  appInfo.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo = &appInfo;
  ici.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  ici.ppEnabledExtensionNames = extensions.data();
  ici.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
  ici.ppEnabledLayerNames = validationLayers.data();

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

  float priority = 1.0f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = graphicsQueueFamily_;
  qci.queueCount = 1;
  qci.pQueuePriorities = &priority;

  const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
  dynamicRenderingFeature.dynamicRendering = VK_TRUE;

  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.enabledExtensionCount = 1;
  dci.ppEnabledExtensionNames = deviceExtensions;
  dci.pNext = &dynamicRenderingFeature;

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

  VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  sci.surface = surface_;
  sci.minImageCount = sc.imageCount;
  sci.imageFormat = sc.format.format;
  sci.imageColorSpace = sc.format.colorSpace;
  sci.imageExtent = sc.extent;
  sci.imageArrayLayers = 1;
  sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  sci.preTransform = caps.currentTransform;
  sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  sci.presentMode = sc.presentMode;
  sci.clipped = VK_TRUE;
  sci.oldSwapchain = oldSwapchain;

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

    VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ivci.image = frames_[i].image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = swapchainFormat_;
    ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(device_, &ivci, nullptr, &frames_[i].view));
  }
}

void Renderer::AllocateFrameCommandsAndSync() {
  std::vector<VkCommandBuffer> commandBuffers(frames_.size(), VK_NULL_HANDLE);
  VkCommandBufferAllocateInfo cbai{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cbai.commandPool = commandPool_;
  cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbai.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
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
  VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  cpci.queueFamilyIndex = graphicsQueueFamily_;
  cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VK_CHECK(vkCreateCommandPool(device_, &cpci, nullptr, &commandPool_));

  VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

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

  VkImageMemoryBarrier toColor{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  toColor.oldLayout = frame.initialized ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                        : VK_IMAGE_LAYOUT_UNDEFINED;
  toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toColor.image = frame.image;
  toColor.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  toColor.subresourceRange.levelCount = 1;
  toColor.subresourceRange.layerCount = 1;
  toColor.srcAccessMask = 0;
  toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &toColor);
  frame.initialized = true;

  VkClearValue clearColor{};
  clearColor.color = {{0.08f, 0.09f, 0.11f, 1.0f}};

  VkRenderingAttachmentInfo colorAttachment{
      VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  colorAttachment.imageView = frame.view;
  colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.clearValue = clearColor;

  VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
  renderingInfo.renderArea.offset = {0, 0};
  renderingInfo.renderArea.extent = swapchainExtent_;
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments = &colorAttachment;

  vkCmdBeginRendering(cmd, &renderingInfo);

  VkViewport viewport{};
  viewport.width = static_cast<float>(swapchainExtent_.width);
  viewport.height = static_cast<float>(swapchainExtent_.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = swapchainExtent_;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  if (drawFn) drawFn(cmd);

  vkCmdEndRendering(cmd);

  VkImageMemoryBarrier toPresent{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toPresent.image = frame.image;
  toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  toPresent.subresourceRange.levelCount = 1;
  toPresent.subresourceRange.layerCount = 1;
  toPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  toPresent.dstAccessMask = 0;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &toPresent);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &sync_.imageAvailable;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmd;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &renderFinished;
  VK_CHECK(vkQueueSubmit(graphicsQueue_, 1, &submitInfo, sync_.inFlight));

  VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &renderFinished;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain_;
  presentInfo.pImageIndices = &imageIndex;

  VkResult present = vkQueuePresentKHR(graphicsQueue_, &presentInfo);
  if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR) {
    RecreateSwapchain();
    return;
  }
  if (present != VK_SUCCESS) VK_CHECK(present);
}
