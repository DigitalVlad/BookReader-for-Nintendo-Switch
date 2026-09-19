#include "render/Fonts.hpp"
#include "utils/Files.hpp"
#include <dirent.h>
#include <stdexcept>
namespace br {
Fonts::Fonts(const std::vector<FontSource>& sources) {
    if (FT_Init_FreeType(&lib_))
        throw std::runtime_error("Cannot initialize FreeType");
    for (const auto& s : sources) {
        FT_Face f = nullptr;
        int err = s.data ? FT_New_Memory_Face(lib_, s.data, s.size, 0, &f)
                         : FT_New_Face(lib_, s.path.c_str(), 0, &f);
        if (!err) {
            FT_Select_Charmap(f, FT_ENCODING_UNICODE);
            faces_.push_back(f);
        }
    }
}
Fonts::~Fonts() {
    for (auto f : faces_)
        FT_Done_Face(f);
    if (lib_)
        FT_Done_FreeType(lib_);
}
FT_Face Fonts::face(uint32_t c) {
    for (auto f : faces_)
        if (FT_Get_Char_Index(f, c))
            return f;
    return faces_.empty() ? nullptr : faces_[0];
}
bool Fonts::hasGlyph(uint32_t cp) const {
    for (auto f : faces_)
        if (FT_Get_Char_Index(f, cp))
            return true;
    return false;
}
void Fonts::size(int p) {
    if (p == pixels_)
        return;
    pixels_ = p;
    advances_.clear();
    for (auto f : faces_)
        FT_Set_Pixel_Sizes(f, 0, p);
}
int Fonts::advance(uint32_t c) {
    auto it = advances_.find(c);
    if (it != advances_.end())
        return it->second;
    auto f = face(c);
    int a = pixels_ / 2;
    if (f && !FT_Load_Char(f, c, FT_LOAD_DEFAULT))
        a = int(f->glyph->advance.x >> 6);
    if (advances_.size() > 8192)
        advances_.clear();
    advances_[c] = a;
    return a;
}
int Fonts::width(const std::string& s) {
    int w = 0;
    size_t i = 0;
    while (i < s.size())
        w += advance(utf8Next(s, i));
    return w;
}
void fill(Bitmap& b, int w, int h, Color c) {
    if (w <= 0 || h <= 0 || uint64_t(w) * h > 8 * 1024 * 1024)
        throw std::runtime_error("Not enough memory: bitmap limit");
    b.width = w;
    b.height = h;
    b.rgba.resize(size_t(w) * h * 4);
    for (size_t i = 0; i < b.rgba.size(); i += 4) {
        b.rgba[i] = c.r;
        b.rgba[i + 1] = c.g;
        b.rgba[i + 2] = c.b;
        b.rgba[i + 3] = 255;
    }
}
void Fonts::draw(Bitmap& dst, int x, int baseline, const std::string& s, Color c, int maxWidth) {
    size_t i = 0;
    int start = x;
    while (i < s.size()) {
        auto cp = utf8Next(s, i);
        int a = advance(cp);
        if (x + a > start + maxWidth)
            break;
        auto f = face(cp);
        if (f && !FT_Load_Char(f, cp, FT_LOAD_RENDER)) {
            auto& g = *f->glyph;
            for (unsigned row = 0; row < g.bitmap.rows; ++row)
                for (unsigned col = 0; col < g.bitmap.width; ++col) {
                    int dx = x + g.bitmap_left + int(col), dy = baseline - g.bitmap_top + int(row);
                    if (dx < 0 || dy < 0 || dx >= dst.width || dy >= dst.height)
                        continue;
                    int pitch = g.bitmap.pitch;
                    auto* src = g.bitmap.buffer +
                                (pitch >= 0 ? row : g.bitmap.rows - 1 - row) * std::abs(pitch);
                    unsigned alpha = g.bitmap.pixel_mode == FT_PIXEL_MODE_MONO
                                         ? ((src[col >> 3] & (0x80 >> (col & 7))) ? 255 : 0)
                                         : src[col];
                    auto* p = &dst.rgba[(size_t(dy) * dst.width + dx) * 4];
                    p[0] = uint8_t((p[0] * (255 - alpha) + c.r * alpha) / 255);
                    p[1] = uint8_t((p[1] * (255 - alpha) + c.g * alpha) / 255);
                    p[2] = uint8_t((p[2] * (255 - alpha) + c.b * alpha) / 255);
                }
        }
        x += a;
    }
}
std::vector<FontSource> discoverFonts(const std::string& root) {
    std::vector<FontSource> r;
    auto p = root + "/fonts";
    DIR* d = opendir(p.c_str());
    if (d) {
        while (auto* e = readdir(d)) {
            auto ext = extension(e->d_name);
            if (ext == "ttf" || ext == "otf" || ext == "ttc")
                r.push_back({p + "/" + e->d_name, nullptr, 0});
        }
        closedir(d);
    }
    std::sort(r.begin(), r.end(), [](auto& a, auto& b) { return a.path < b.path; });
    if (r.size() > 32)
        r.resize(32);
    return r;
}
} // namespace br
