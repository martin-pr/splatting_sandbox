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
  Renderer renderer(app.getWindow());

  std::optional<ImageLayer> imageLayer;
  if (argc > 1) {
    imageLayer.emplace(renderer.getContext(), argv[1]);
  }

  TriangleLayer triangleLayer(renderer.getContext());
  ImGuiLayer imguiLayer(app.getWindow(), renderer.getContext());

  bool showTriangle = true;
  bool showImage = true;

  bool running = true;
  while (running) {
    running = app.pollEvents([&](const SDL_Event& e) {
      imguiLayer.processEvent(e);

      if (e.type == SDL_EVENT_WINDOW_RESIZED ||
          e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        renderer.recreateSwapchain();

        ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(e.window.data1),
                                          static_cast<float>(e.window.data2));
      }
    });

    renderer.renderFrame([&](VkCommandBuffer cmd) {
      if (imageLayer.has_value() && showImage) {
        imageLayer->render(cmd, renderer.getSwapchainExtent());
      }

      if (showTriangle) {
        triangleLayer.render(cmd);
      }

      imguiLayer.render(cmd, [&]() {
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
