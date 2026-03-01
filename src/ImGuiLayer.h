#pragma once

#include <SDL3/SDL.h>

#include <functional>

#include "LayerBase.h"

class ImGuiLayer : public LayerBase {
 public:
  ImGuiLayer(SDL_Window* window, const Renderer::Context& ctx);
  ~ImGuiLayer();

  void ProcessEvent(const SDL_Event& event);
  void Render(VkCommandBuffer cmd, const std::function<void()>& uiFn);

 private:
  void Destroy();

  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  bool initialized_ = false;
};
