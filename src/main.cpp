#include <imgui.h>

#include <iostream>

#include "App.h"
#include "ImGuiLayer.h"
#include "Renderer.h"
#include "TriangleLayer.h"

int main() {
  App app;
  Renderer renderer(app.GetWindow());
  TriangleLayer triangleLayer(renderer.GetContext());
  ImGuiLayer imguiLayer(app.GetWindow(), renderer.GetContext());

  bool running = true;
  while (running) {
    running = app.PollEvents([&](const SDL_Event& e) {
      imguiLayer.ProcessEvent(e);

      if (e.type == SDL_EVENT_WINDOW_RESIZED ||
          e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        // Resizing requires new swapchain
        renderer.RecreateSwapchain();

        // Keep ImGui coordinates in sync with the actual swapchain pixel size.
        int width = e.window.data1;
        int height = e.window.data2;

        ImGui::GetIO().DisplaySize = ImVec2(width, height);
      }
    });

    renderer.RenderFrame([&](VkCommandBuffer cmd) {
      triangleLayer.Render(cmd, renderer.GetSwapchainExtent());

      imguiLayer.Render(cmd, []() {
        ImGui::Begin("Hello");
        ImGui::Text("SDL3 + Vulkan + ImGui");
        ImGui::Text("Triangle is rendered with Vulkan pipeline.");
        ImGui::End();
      });
    });
  }

  return 0;
}
