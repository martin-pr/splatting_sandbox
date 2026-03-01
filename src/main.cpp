#include <imgui.h>

#include <iostream>
#include <optional>

#include "App.h"
#include "ImGuiLayer.h"
#include "ImageLayer.h"
#include "Renderer.h"
#include "TriangleLayer.h"

int main(int argc, char* argv[]) {
  App app;
  Renderer renderer(app.GetWindow());

  std::optional<ImageLayer> imageLayer;
  if (argc > 1) {
    imageLayer.emplace(renderer.GetContext(), argv[1]);
  }

  TriangleLayer triangleLayer(renderer.GetContext());
  ImGuiLayer imguiLayer(app.GetWindow(), renderer.GetContext());

  bool showTriangle = true;
  bool showImage = true;

  bool running = true;
  while (running) {
    running = app.PollEvents([&](const SDL_Event& e) {
      imguiLayer.ProcessEvent(e);

      if (e.type == SDL_EVENT_WINDOW_RESIZED ||
          e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        // Resizing requires new swapchain
        renderer.RecreateSwapchain();

        // Keep ImGui coordinates in sync with the actual swapchain pixel size.
        ImGui::GetIO().DisplaySize = ImVec2(e.window.data1, e.window.data2);
      }
    });

    renderer.RenderFrame([&](VkCommandBuffer cmd) {
      if (imageLayer.has_value() && showImage) {
        imageLayer->Render(cmd, renderer.GetSwapchainExtent());
      }

      if (showTriangle) {
        triangleLayer.Render(cmd);
      }

      imguiLayer.Render(cmd, [&]() {
        ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_FirstUseEver);
        ImGui::Begin("Layers");
        ImGui::Checkbox("Triangle", &showTriangle);
        if (imageLayer.has_value()) {
          ImGui::Checkbox("Image", &showImage);
        }
        ImGui::End();
      });
    });
  }

  return 0;
}
