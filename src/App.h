#pragma once

#include <SDL3/SDL.h>

#include <functional>

class App {
 public:
  App();
  ~App();

  App(const App&) = delete;
  App& operator=(const App&) = delete;
  App(App&&) = delete;
  App& operator=(App&&) = delete;

  bool pollEvents(const std::function<void(const SDL_Event&)>& handler = {});
  [[nodiscard]] SDL_Window* getWindow() const;

 private:
  SDL_Window* window_;
};
