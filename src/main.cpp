#include <iostream>

#include "App.h"
#include "Renderer.h"

int main() {
  try {
    App app;
    Renderer renderer(app.GetWindow());

    while (app.PollEvents()) {
      renderer.RenderFrame();
    }
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << "\n";
    return -1;
  }

  return 0;
}
