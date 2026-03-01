#pragma once

#include <SDL3/SDL.h>

#include <functional>

#include "LayerBase.h"

class ImGuiLayer : public LayerBase {
 public:
  ImGuiLayer(SDL_Window* window, const Renderer::Context& ctx);
  ~ImGuiLayer();

  ImGuiLayer(const ImGuiLayer&) = delete;
  ImGuiLayer& operator=(const ImGuiLayer&) = delete;
  ImGuiLayer(ImGuiLayer&&) = delete;
  ImGuiLayer& operator=(ImGuiLayer&&) = delete;

  void processEvent(const SDL_Event& event);
  void render(VkCommandBuffer cmd, const std::function<void()>& uiFn);

 private:
  void destroy();

  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  bool initialized_ = false;
};
