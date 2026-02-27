#include "App.h"

#include <iostream>
#include <stdexcept>

App::App() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    throw std::runtime_error(SDL_GetError());
  }

  window_ = SDL_CreateWindow("Splatting Sandbox", 1280, 720,
                             SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window_) {
    throw std::runtime_error(SDL_GetError());
  }
}

App::~App() {
  SDL_DestroyWindow(window_);
  SDL_Quit();
}

bool App::PollEvents(const std::function<void(const SDL_Event&)>& handler) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (handler) {
      handler(event);
    }

    if (event.type == SDL_EVENT_QUIT) {
      return false;
    };
  }

  return true;
}

SDL_Window* App::GetWindow() const {
  return window_;
}
