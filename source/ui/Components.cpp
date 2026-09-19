#include "ui/Components.hpp"
namespace br {
Components::~Components() {
    for (auto& t : textCache_)
        SDL_DestroyTexture(t.texture);
}
void Components::begin(int theme) {
    colors = palette(theme);
    buttons.clear();
    SDL_SetRenderDrawColor(renderer_, colors.background.r, colors.background.g, colors.background.b,
                           255);
    SDL_RenderClear(renderer_);
}
void Components::rect(SDL_Rect r, Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer_, &r);
}
void Components::text(const std::string& value, int x, int y, int size, Color color, int maxWidth) {
    if (value.empty())
        return;
    std::string key = value + "|" + std::to_string(size) + ":" + std::to_string(color.r) + ":" +
                      std::to_string(color.g) + ":" + std::to_string(color.b) + ":" +
                      std::to_string(maxWidth);
    auto it =
        std::find_if(textCache_.begin(), textCache_.end(), [&](auto& t) { return t.key == key; });
    if (it == textCache_.end()) {
        fonts_.size(size);
        Bitmap b;
        int w = std::min(maxWidth, fonts_.width(value) + 4), h = size * 2;
        fill(b, std::max(1, w), h, colors.background);
        fonts_.draw(b, 0, size, value, color,
                    maxWidth); // Turn background into transparency; retain antialias coverage.
        for (size_t p = 0; p < b.rgba.size(); p += 4) {
            int channel = 0;
            int best = 0;
            const uint8_t bg[] = {colors.background.r, colors.background.g, colors.background.b},
                          fg[] = {color.r, color.g, color.b};
            for (int c = 0; c < 3; ++c)
                if (std::abs(int(fg[c]) - bg[c]) > best) {
                    channel = c;
                    best = std::abs(int(fg[c]) - bg[c]);
                }
            int a = best ? int((int(b.rgba[p + channel]) - bg[channel]) * 255 /
                               (int(fg[channel]) - bg[channel]))
                         : 0;
            b.rgba[p] = color.r;
            b.rgba[p + 1] = color.g;
            b.rgba[p + 2] = color.b;
            b.rgba[p + 3] = uint8_t(std::clamp(a, 0, 255));
        }
        SDL_Texture* t = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STATIC, b.width, b.height);
        if (!t)
            return;
        SDL_UpdateTexture(t, nullptr, b.rgba.data(), b.width * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        textCache_.push_front({key, t, b.width, b.height});
        while (textCache_.size() > 96) {
            SDL_DestroyTexture(textCache_.back().texture);
            textCache_.pop_back();
        }
        it = textCache_.begin();
    }
    SDL_Rect dst{x, y, it->w, it->h};
    SDL_RenderCopy(renderer_, it->texture, nullptr, &dst);
    textCache_.splice(textCache_.begin(), textCache_, it);
}
void Components::button(SDL_Rect r, const std::string& label, std::function<void()> action,
                        bool selected) {
    rect(r, selected ? colors.accent : colors.panel);
    int size = 24;
    fonts_.size(size);
    while (size > 18 && fonts_.width(label) > r.w - 28)
        fonts_.size(--size);
    text(label, r.x + 14, r.y + (r.h - size) / 2 - 2, size, colors.text, r.w - 28);
    buttons.push_back({r, label, std::move(action)});
}
bool Components::tap(float x, float y) {
    for (auto& b : buttons)
        if (x >= b.rect.x && y >= b.rect.y && x < b.rect.x + b.rect.w && y < b.rect.y + b.rect.h) {
            auto action = b.action;
            action();
            return true;
        }
    return false;
}
void Components::focus(int index) {
    if (buttons.empty())
        return;
    index = (index % int(buttons.size()) + int(buttons.size())) % int(buttons.size());
    auto r = buttons[index].rect;
    SDL_SetRenderDrawColor(renderer_, colors.accent.r, colors.accent.g, colors.accent.b, 255);
    for (int i = 0; i < 3; ++i) {
        SDL_RenderDrawRect(renderer_, &r);
        ++r.x;
        ++r.y;
        r.w -= 2;
        r.h -= 2;
    }
}
} // namespace br
