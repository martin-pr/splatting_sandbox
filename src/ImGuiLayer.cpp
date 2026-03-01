#include "ImGuiLayer.h"

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <stdexcept>

ImGuiLayer::~ImGuiLayer() {
  destroy();
}

ImGuiLayer::ImGuiLayer(SDL_Window* window, const Renderer::Context& ctx)
    : LayerBase(ctx) {
  try {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForVulkan(window);

    const VkDescriptorPoolSize poolSize = {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000};
    const VkDescriptorPoolCreateInfo dpci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };
    if (vkCreateDescriptorPool(device_, &dpci, nullptr, &pool_) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateDescriptorPool failed");
    }

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = ctx.instance;
    initInfo.PhysicalDevice = ctx.physicalDevice;
    initInfo.Device = ctx.device;
    initInfo.QueueFamily = ctx.queueFamily;
    initInfo.Queue = ctx.graphicsQueue;
    initInfo.DescriptorPool = pool_;
    initInfo.RenderPass = VK_NULL_HANDLE;
    initInfo.MinImageCount = ctx.imageCount;
    initInfo.ImageCount = ctx.imageCount;
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats =
        &ctx.swapchainFormat;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
      throw std::runtime_error("ImGui_ImplVulkan_Init failed");
    }

    initialized_ = true;
  } catch (...) {
    destroy();
    throw;
  }
}

void ImGuiLayer::processEvent(const SDL_Event& event) {
  ImGui_ImplSDL3_ProcessEvent(&event);
}

void ImGuiLayer::render(VkCommandBuffer cmd,
                        const std::function<void()>& uiFn) {
  ImGui_ImplSDL3_NewFrame();
  ImGui_ImplVulkan_NewFrame();
  ImGui::NewFrame();

  uiFn();

  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void ImGuiLayer::destroy() {
  if (initialized_) {
    vkDeviceWaitIdle(device_);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
  }
  if (pool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device_, pool_, nullptr);
    pool_ = VK_NULL_HANDLE;
  }
  device_ = VK_NULL_HANDLE;
}
