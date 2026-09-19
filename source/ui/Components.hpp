#pragma once
#include "render/Fonts.hpp"
#include <SDL.h>
#include <functional>
#include <list>
#include <stdexcept>
namespace br {
struct Button {
    SDL_Rect rect;
    std::string label;
    std::function<void()> action;
};
class Components {
    SDL_Renderer* renderer_;
    Fonts fonts_;
    struct Text {
        std::string key;
        SDL_Texture* texture;
        int w, h;
    };
    std::list<Text> textCache_;

  public:
    std::vector<Button> buttons;
    Palette colors;
    Components(SDL_Renderer* renderer, const std::vector<FontSource>& fonts)
        : renderer_(renderer), fonts_(fonts) {
        if (!fonts_.valid())
            throw std::runtime_error("No fonts. Add a TTF to /switch/BookReader/fonts/");
    }
    ~Components();
    void begin(int theme);
    void rect(SDL_Rect r, Color color);
    void text(const std::string& value, int x, int y, int size, Color color, int maxWidth = 1200);
    void button(SDL_Rect r, const std::string& text, std::function<void()> action,
                bool selected = false);
    bool tap(float x, float y);
    void focus(int index);
    SDL_Renderer* renderer() const { return renderer_; }
};
} // namespace br
