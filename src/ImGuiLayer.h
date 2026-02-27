#pragma once

#include <SDL3/SDL.h>

#include <functional>

#include "Renderer.h"

class ImGuiLayer {
 public:
  ImGuiLayer(SDL_Window* window, const Renderer::Context& ctx);
  ~ImGuiLayer();

  ImGuiLayer(const ImGuiLayer&) = delete;
  ImGuiLayer& operator=(const ImGuiLayer&) = delete;

  void ProcessEvent(const SDL_Event& event);
  void Render(VkCommandBuffer cmd, const std::function<void()>& uiFn);

 private:
  void Destroy();

  VkDevice device_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  bool initialized_ = false;
};
