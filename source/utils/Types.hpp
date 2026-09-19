#pragma once
#include "ui/Localization.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace br {
struct Color {
    uint8_t r, g, b, a = 255;
};
struct Palette {
    Color background, text, panel, accent, muted;
};
inline Palette palette(int n) {
    if (n == 1)
        return {{236, 222, 191}, {64, 49, 36}, {218, 201, 166}, {135, 85, 39}, {112, 94, 72}};
    if (n == 2)
        return {{18, 20, 23}, {204, 208, 212}, {30, 34, 40}, {93, 185, 174}, {145, 156, 165}};
    return {{246, 247, 249}, {31, 39, 49}, {225, 231, 237}, {33, 113, 128}, {92, 106, 119}};
}
struct Settings {
    Language language = Language::English;
    int fontSize = 28, margin = 40, theme = 0, orientation = 0, font = 0, alignment = 0;
    float lineSpacing = 1.5f, brightness = 1.f;
    bool scrollMode = false;
    void clamp() {
        fontSize = std::clamp(fontSize, 16, 56);
        margin = std::clamp(margin, 16, 100);
        theme = std::clamp(theme, 0, 2);
        orientation = std::clamp(orientation, 0, 1);
        font = std::clamp(font, 0, 32);
        alignment = std::clamp(alignment, 0, 2);
        lineSpacing = std::clamp(lineSpacing, 1.f, 2.f);
        brightness = std::clamp(brightness, .25f, 1.f);
    }
    bool operator==(const Settings& s) const {
        return fontSize == s.fontSize && margin == s.margin && theme == s.theme &&
               orientation == s.orientation && font == s.font && alignment == s.alignment &&
               lineSpacing == s.lineSpacing && brightness == s.brightness &&
               scrollMode == s.scrollMode && language == s.language;
    }
    int width() const { return orientation ? 720 : 1280; }
    int height() const { return orientation ? 1280 : 720; }
};
struct Position {
    int page = 0, chapter = 0;
    uint64_t offset = 0;
    float zoom = 1, panX = 0, panY = 0, progress = 0;
};
struct Chapter {
    std::string title;
    int page = 0, level = 0;
};
struct Bitmap {
    int width = 0, height = 0;
    std::vector<uint8_t> rgba;
};
struct Book {
    std::string path, title, author, format;
    int64_t modified = 0, opened = 0;
    bool favorite = false;
    Position position;
    Settings settings;
    bool hasSettings = false;
};
} // namespace br
