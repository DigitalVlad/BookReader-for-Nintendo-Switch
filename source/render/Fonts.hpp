#pragma once
#include "utils/Types.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>
namespace br {
struct FontSource {
    std::string path;
    const unsigned char* data = nullptr;
    long size = 0;
};
class Fonts {
    FT_Library lib_ = nullptr;
    std::vector<FT_Face> faces_;
    int pixels_ = 0;
    std::map<uint32_t, int> advances_;
    FT_Face face(uint32_t cp);

  public:
    explicit Fonts(const std::vector<FontSource>& sources);
    ~Fonts();
    Fonts(const Fonts&) = delete;
    Fonts& operator=(const Fonts&) = delete;
    bool valid() const { return !faces_.empty(); }
    bool hasGlyph(uint32_t cp) const;
    void size(int pixels);
    int advance(uint32_t cp);
    int width(const std::string& text);
    void draw(Bitmap& dst, int x, int baseline, const std::string& text, Color color,
              int maxWidth = 100000);
};
void fill(Bitmap& bitmap, int w, int h, Color color);
std::vector<FontSource> discoverFonts(const std::string& root);
} // namespace br
