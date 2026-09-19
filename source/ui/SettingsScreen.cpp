#include "app/App.hpp"
#include "utils/Files.hpp"
namespace br {
SDL_Rect App::sliderRect(int i) const {
    return settings_.orientation ? SDL_Rect{142, 184 + i * 126, width() - 284, 60}
                                 : SDL_Rect{490, 148 + i * 82, 560, 60};
}
void App::setSlider(int index, float x) {
    auto r = sliderRect(index);
    float t = std::clamp((x - r.x) / float(r.w), 0.f, 1.f);
    if (index == 0)
        settings_.fontSize = int(16 + t * 40);
    if (index == 1)
        settings_.lineSpacing = 1 + t;
    if (index == 2)
        settings_.margin = int(16 + t * 84);
    if (index == 3)
        settings_.brightness = .25f + t * .75f;
    settingsDirty_ = true;
}
void App::changeOrientation() {
    settings_.orientation ^= 1;
    panX_ = panY_ = inertia_ = 0;
    tapDeadline_ = zoomDeadline_ = 0;
    gestures_ = Gestures{};
    touchSuppressed_ = true;
    slider_ = -1;
    applySettings();
    config_.save(monotonic(), true);
}
void App::drawSettings() {
    auto& u = *ui_;
    const bool portrait = settings_.orientation;
    u.text(tr("Settings"), 40, 25, 36, u.colors.text, width() - 250);
    u.button({width() - 200, 24, 160, 60}, tr("Done"), [this] { leaveSettings(); });
    std::string labels[] = {
        tr("Font size") + ": " + std::to_string(settings_.fontSize),
        tr("Line spacing") + ": " + std::to_string(int(settings_.lineSpacing * 100)) + "%",
        tr("Margins") + ": " + std::to_string(settings_.margin),
        tr("Brightness") + ": " + std::to_string(int(settings_.brightness * 100)) + "%"};
    float values[] = {(settings_.fontSize - 16) / 40.f, settings_.lineSpacing - 1,
                      (settings_.margin - 16) / 84.f, (settings_.brightness - .25f) / .75f};
    for (int i = 0; i < 4; ++i) {
        auto r = sliderRect(i);
        u.text(labels[i], 42, portrait ? r.y - 40 : r.y + 12, 26, u.colors.text,
               portrait ? width() - 84 : 335);
        auto step = [this, i](int d) {
            if (i == 0)
                settings_.fontSize += d * 2;
            if (i == 1)
                settings_.lineSpacing += d * .1f;
            if (i == 2)
                settings_.margin += d * 4;
            if (i == 3)
                settings_.brightness += d * .05f;
            settings_.clamp();
            settingsDirty_ = true;
        };
        u.button({portrait ? 42 : 385, r.y, 80, 60}, "-", [step] { step(-1); });
        u.rect({r.x, r.y + 27, r.w, 8}, u.colors.panel);
        u.rect({r.x, r.y + 27, int(r.w * values[i]), 8}, u.colors.accent);
        u.rect({r.x - 12 + int(r.w * values[i]), r.y + 8, 24, 48}, u.colors.accent);
        u.button({portrait ? width() - 122 : 1090, r.y, 80, 60}, "+", [step] { step(1); });
    }
    // Two columns in portrait, four in landscape. Each choice has its own label.
    int columns = portrait ? 2 : 4, cell = (width() - 80 - 16 * (columns - 1)) / columns;
    int start = portrait ? 700 : 488;
    int idx = 0;
    auto choice = [&](const std::string& label, const std::string& value,
                      std::function<void()> action) {
        int x = 40 + (idx % columns) * (cell + 16), y = start + (idx / columns) * 94;
        ++idx;
        u.text(label, x, y, 20, u.colors.muted, cell);
        u.button({x, y + 29, cell, 56}, value, std::move(action));
    };
    choice(tr("Language"), settings_.language == Language::Russian ? "Русский" : "English", [this] {
        settings_.language =
            settings_.language == Language::English ? Language::Russian : Language::English;
        keyboardRussian_ = settings_.language == Language::Russian;
        config_.settings.language = settings_.language;
        config_.changed(monotonic());
        config_.save(monotonic(), true);
    });
    choice(tr("Orientation"), tr(portrait ? "Vertical" : "Horizontal"),
           [this] { changeOrientation(); });
    const char* themes[] = {"Light", "Sepia", "Dark"};
    choice(tr("Theme"), tr(themes[settings_.theme]), [this] {
        settings_.theme = (settings_.theme + 1) % 3;
        settingsDirty_ = true;
    });
    choice(tr("Reading mode"), tr(settings_.scrollMode ? "Scroll" : "Pages"), [this] {
        settings_.scrollMode = !settings_.scrollMode;
        settingsDirty_ = true;
    });
    bool text =
        activePath_.empty() || extension(activePath_) == "txt" || extension(activePath_) == "md";
    std::string font = text ? (settings_.font == 0 ? tr("Auto")
                               : size_t(settings_.font - 1) < fonts_.size() &&
                                       !fonts_[settings_.font - 1].path.empty()
                                   ? basename(fonts_[settings_.font - 1].path)
                                   : tr("System"))
                            : tr(settings_.font % 3 == 0   ? "Serif"
                                 : settings_.font % 3 == 1 ? "Sans"
                                                           : "Mono");
    choice(tr("Font"), font, [this, text] {
        settings_.font = (settings_.font + 1) % (text ? std::max(1, int(fonts_.size() + 1)) : 3);
        settingsDirty_ = true;
    });
    const char* align[] = {"Left", "Center", "Right"};
    choice(tr("Alignment"), tr(align[settings_.alignment]), [this] {
        settings_.alignment = (settings_.alignment + 1) % 3;
        settingsDirty_ = true;
    });
    if (portrait) {
        u.text(tr("PDF layout is fixed."), 40, 1030, 22, u.colors.muted, width() - 80);
        u.text(tr("Brightness dims this app only."), 40, 1070, 22, u.colors.muted, width() - 80);
    } else {
        u.text(tr("PDF layout is fixed."), 656, 604, 20, u.colors.muted, 570);
        u.text(tr("Brightness dims this app only."), 656, 638, 20, u.colors.muted, 570);
    }
}
} // namespace br
