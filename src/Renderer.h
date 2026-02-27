#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <vector>

class Renderer {
 public:
  struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t queueFamily = UINT32_MAX;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    uint32_t imageCount = 0;
  };

  explicit Renderer(SDL_Window* window);
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void RenderFrame(const std::function<void(VkCommandBuffer)>& drawFn = {});

  void RecreateSwapchain();
  Context GetContext() const;

 private:
  struct FrameSync {
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkFence inFlight = VK_NULL_HANDLE;
  };

  struct SwapchainImageResources {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    bool initialized = false;
  };

  struct SwapchainConfig {
    VkSurfaceFormatKHR format{};
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D extent{};
    uint32_t imageCount = 2;
  };

  void InitInstanceAndSurface();
  void InitDeviceAndSwapchain();
  void InitCommandsAndSync();

  void CreateSwapchain(VkSwapchainKHR oldSwapchain);
  void AllocateFrameCommandsAndSync();
  void DestroySwapchainResources();

  void Destroy();

  static bool HasInstanceLayer(const char* layerName);
  static VkPhysicalDevice SelectPhysicalDevice(
      const std::vector<VkPhysicalDevice>& devices);
  static SwapchainConfig SelectSwapchainConfig(
      SDL_Window* window, const VkSurfaceCapabilitiesKHR& caps,
      const std::vector<VkSurfaceFormatKHR>& formats,
      const std::vector<VkPresentModeKHR>& modes);

  SDL_Window* window_ = nullptr;

  VkInstance instance_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphicsQueue_ = VK_NULL_HANDLE;
  uint32_t graphicsQueueFamily_ = UINT32_MAX;

  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
  VkExtent2D swapchainExtent_{};
  std::vector<SwapchainImageResources> frames_;

  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  FrameSync sync_;
};
