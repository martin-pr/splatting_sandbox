#pragma once

#include <SDL3/SDL.h>

class App {
 public:
  App();
  ~App();

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  bool PollEvents();
  SDL_Window* GetWindow() const;

 private:
  void Destroy();

  SDL_Window* window_ = nullptr;
};
