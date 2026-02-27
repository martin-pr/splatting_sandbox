#include <imgui.h>

#include <iostream>

#include "App.h"
#include "ImGuiLayer.h"
#include "Renderer.h"

int main() {
  App app;
  Renderer renderer(app.GetWindow());
  ImGuiLayer imguiLayer(app.GetWindow(), renderer.GetContext());

  bool running = true;
  while (running) {
    running =
        app.PollEvents([&](const SDL_Event& e) { imguiLayer.ProcessEvent(e); });

    renderer.RenderFrame([&](VkCommandBuffer cmd) {
      imguiLayer.Render(cmd, []() {
        ImGui::Begin("Hello");
        ImGui::Text("SDL3 + Vulkan + ImGui");
        ImGui::End();
      });
    });
  }

  return 0;
}
