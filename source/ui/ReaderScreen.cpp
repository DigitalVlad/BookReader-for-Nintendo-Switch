#include "app/App.hpp"
#include <cmath>
namespace br {
void App::drawReader() {
    auto& u = *ui_;
    if (pageTexture_ && view_.bitmap) {
        auto page = [&](SDL_Texture* t, const std::shared_ptr<Bitmap>& b, int index) {
            if (!t || !b)
                return;
            float scale = std::min(float(settings_.width()) / b->width,
                                   float(settings_.height()) / b->height);
            if (view_.fixed)
                scale *= zoom_;
            int w = int(b->width * scale), h = int(b->height * scale);
            float offset = panY_ + index * settings_.height();
            SDL_Rect dest{width() / 2 - w / 2 + int(panX_), height() / 2 - h / 2 + int(offset), w,
                          h};
            SDL_RenderCopy(renderer_, t, nullptr, &dest);
        };
        page(pageTexture_, view_.bitmap, 0);
        if (!view_.fixed && settings_.scrollMode) {
            page(previousTexture_, view_.previous, -1);
            page(nextTexture_, view_.next, 1);
        }
    }

    if (!overlay_)
        return;
    u.rect({0, 0, width(), height()},
           {u.colors.background.r, u.colors.background.g, u.colors.background.b, 245});
    u.text(view_.title.empty() ? tr("Book") : view_.title, 40, 24, 32, u.colors.text, width() - 80);
    std::string chapterName;
    for (auto& c : view_.chapters)
        if (c.page <= view_.position.page)
            chapterName = c.title;
    u.text(tr("Chapter") + ": " + chapterName, 40, 76, 24, u.colors.muted, width() - 80);
    u.text(tr("Page") + " " + std::to_string(view_.position.page + 1) + " / " +
               std::to_string(view_.pages) + "   " + tr("Progress") + ": " +
               std::to_string(int(view_.position.progress * 100)) + "%",
           40, 116, 24, u.colors.text, width() - 80);
    u.rect({40, 158, width() - 80, 8}, u.colors.panel);
    u.rect({40, 158, int((width() - 80) * view_.position.progress), 8}, u.colors.accent);
    int columns = settings_.orientation ? 2 : 4, cell = (width() - 80) / columns;
    auto control = [&](int i) {
        return SDL_Rect{40 + (i % columns) * cell, 206 + (i / columns) * 94, cell - 16, 64};
    };
    u.button(control(0), tr(view_.fixed ? "Zoom -" : "Font -"), [this] {
        if (view_.fixed)
            setZoom(zoom_ - .25f);
        else {
            settings_.fontSize -= 2;
            applySettings();
        }
    });
    u.button(control(1), tr(view_.fixed ? "Zoom +" : "Font +"), [this] {
        if (view_.fixed)
            setZoom(zoom_ + .25f);
        else {
            settings_.fontSize += 2;
            applySettings();
        }
    });
    u.button(control(2), tr("Theme"), [this] {
        settings_.theme = (settings_.theme + 1) % 3;
        applySettings();
    });
    u.button(control(3), tr("Contents"), [this] {
        setScreen(Screen::Contents);
        tocStart_ = 0;
    });
    u.button(control(4), tr("Previous"), [this] { navigate(-1); });
    u.button(control(5), tr("Next"), [this] { navigate(1); });
    u.button(control(6), tr("Favorite"), [this] { toggleFavorite(activePath_); });
    u.button(control(7), tr("Settings"), [this] { showSettings(); });
    int resumeY = settings_.orientation ? 622 : 406;
    u.button({40, resumeY, width() - 80, 64}, tr("Resume reading"), [this] { overlay_ = false; });
    u.button({40, resumeY + 78, width() - 80, 64}, tr("Close Book"), [this] { closeBook(); });
    int hintY = settings_.orientation ? 840 : 570;
    u.text(tr("X: menu · B: back · L/R: chapter"), 40, hintY, 22, u.colors.muted, width() - 80);
    u.text(tr("ZL/ZR: 10 pages · +: settings"), 40, hintY + 40, 22, u.colors.muted, width() - 80);
    u.text(tr("Pinch / double tap: zoom · Drag: pan"), 40, hintY + 80, 22, u.colors.muted,
           width() - 80);
}
void App::drawContents() {
    auto& u = *ui_;
    u.text(tr("Contents"), 40, 28, 36, u.colors.text, width() - 280);
    u.button({width() - 220, 25, 180, 58}, tr("Back"), [this] { back(); });
    if (view_.chapters.empty())
        u.text(tr("This book has no table of contents."), 44, 140, 26, u.colors.muted,
               width() - 88);
    for (int row = 0; row < tocRows() && tocStart_ + row < int(view_.chapters.size()); ++row) {
        auto c = view_.chapters[tocStart_ + row];
        u.button({40, 114 + row * 70, width() - 80, 58},
                 std::to_string(c.page + 1) + "   " + std::string(std::min(c.level, 4) * 2, ' ') +
                     c.title,
                 [this, c] {
                     Request r;
                     r.kind = Request::Jump;
                     r.delta = c.page;
                     worker_->submit(r);
                     panX_ = panY_ = 0;
                     setScreen(Screen::Reader);
                     overlay_ = false;
                 });
    }
    u.button({40, height() - 86, 220, 58}, tr("Previous"),
             [this] { tocStart_ = std::max(0, tocStart_ - tocRows()); });
    u.button({280, height() - 86, 220, 58}, tr("Next"), [this] {
        tocStart_ =
            std::min(std::max(0, int(view_.chapters.size()) - tocRows()), tocStart_ + tocRows());
    });
}
} // namespace br
