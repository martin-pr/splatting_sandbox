#pragma once

#include <SDL3/SDL.h>

#include <functional>

class App {
 public:
  App();
  ~App();

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  bool PollEvents(const std::function<void(const SDL_Event&)>& handler = {});
  SDL_Window* GetWindow() const;

 private:
  SDL_Window* window_;
};
