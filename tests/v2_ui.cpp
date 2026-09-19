#include "app/App.hpp"
#include "utils/Files.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
        ++checks;                                                                                  \
    } while (0)
static int checks = 0;
namespace br {
class AppTest {
    static void frame(App& app) {
        app.collectView();
        app.draw();
    }
    static void bounds(App& a) {
        frame(a);
        for (const auto& b : a.ui_->buttons) {
            REQUIRE(b.rect.x >= 0 && b.rect.y >= 0);
            REQUIRE(b.rect.x + b.rect.w <= a.width() && b.rect.y + b.rect.h <= a.height());
            REQUIRE(b.rect.w >= 44 && b.rect.h >= 44);
        }
    }
    static void contact(App& a, Uint32 type, Point physical) {
        SDL_Event e{};
        e.type = type;
        if (type == SDL_MOUSEMOTION) {
            e.motion.x = int(physical.x);
            e.motion.y = int(physical.y);
        } else {
            e.button.x = int(physical.x);
            e.button.y = int(physical.y);
            e.button.button = SDL_BUTTON_LEFT;
        }
        SDL_PushEvent(&e);
        a.input(monotonic(), .016f);
    }
    static void finger(App& a, Uint32 type, long long id, Point logical) {
        auto p = OrientationTransform(a.settings_.orientation).logicalToPhysical(logical);
        SDL_Event e{};
        e.type = type;
        e.tfinger.fingerId = id;
        e.tfinger.touchId = 1;
        e.tfinger.x = p.x / 1280.f;
        e.tfinger.y = p.y / 720.f;
        SDL_PushEvent(&e);
        a.input(monotonic(), .016f);
    }
    static void tap(App& a, Point logical) {
        auto p = OrientationTransform(a.settings_.orientation).logicalToPhysical(logical);
        contact(a, SDL_MOUSEBUTTONDOWN, p);
        contact(a, SDL_MOUSEBUTTONUP, p);
        a.input(monotonic(), .016f);
        frame(a);
    }
    static void button(App& a, const std::string& label) {
        frame(a);
        auto i = std::find_if(a.ui_->buttons.begin(), a.ui_->buttons.end(),
                              [&](auto& b) { return b.label == label; });
        REQUIRE(i != a.ui_->buttons.end());
        auto r = i->rect;
        tap(a, {r.x + r.w * .5f, r.y + r.h * .5f});
    }
    static void capture(App& a, const char* name, bool logical = true) {
        if (logical)
            SDL_SetRenderTarget(a.renderer_, a.canvas_);
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(
            0, logical ? a.width() : 1280, logical ? a.height() : 720, 32, SDL_PIXELFORMAT_RGBA32);
        REQUIRE(s != nullptr);
        REQUIRE(SDL_RenderReadPixels(a.renderer_, nullptr, SDL_PIXELFORMAT_RGBA32, s->pixels,
                                     s->pitch) == 0);
        REQUIRE(SDL_SaveBMP(s, name) == 0);
        SDL_FreeSurface(s);
        SDL_SetRenderTarget(a.renderer_, nullptr);
    }
    static void waitBook(App& a) {
        for (int i = 0; i < 1000; ++i) {
            a.collectView();
            if (a.view_.pages && !a.busy_)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        throw std::runtime_error("Book timeout: " + a.message_);
    }
    static void waitIdle(App& a) {
        for (int i = 0; i < 1000; ++i) {
            a.collectView();
            if (!a.busy_)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        throw std::runtime_error("App timeout: " + a.message_);
    }

  public:
    static void run() {
        char temp[] = "/tmp/bookreader-v2-XXXXXX";
        REQUIRE(mkdtemp(temp) != nullptr);
        setenv("BOOKREADER_ROOT", temp, 1);
        setenv("SDL_VIDEODRIVER", "dummy", 1);
        setenv("BOOKREADER_SMOKE_FRAMES", "3", 1);
        unsetenv("BOOKREADER_OPEN");
        mkdir("screenshots", 0755);
        App a;
        REQUIRE(a.run() == 0);
        for (int orientation : {0, 1}) {
            OrientationTransform t(orientation);
            for (Point p : {Point{0, 0}, Point{float(t.width()), 0}, Point{0, float(t.height())},
                            Point{float(t.width()), float(t.height())}, Point{143, 371}}) {
                auto back = t.physicalToLogical(t.logicalToPhysical(p));
                REQUIRE(std::abs(back.x - p.x) < .01f && std::abs(back.y - p.y) < .01f);
            }
        }
        Fonts fonts(a.fonts_);
        for (uint32_t cp = 0x410; cp <= 0x44f; ++cp)
            REQUIRE(fonts.hasGlyph(cp));
        REQUIRE(fonts.hasGlyph(0x401) && fonts.hasGlyph(0x451));
        button(a, "Settings");
        button(a, "English");
        REQUIRE(a.settings_.language == Language::Russian);
        button(a, "Горизонтальная");
        REQUIRE(a.width() == 720 && a.height() == 1280);
        bounds(a);
        // Slider drag in physical coordinates must update the logical brightness control.
        auto r = a.sliderRect(3);
        auto transform = OrientationTransform(1);
        contact(a, SDL_MOUSEBUTTONDOWN, transform.logicalToPhysical({float(r.x), float(r.y + 30)}));
        contact(a, SDL_MOUSEMOTION,
                transform.logicalToPhysical({r.x + r.w * .5f, float(r.y + 30)}));
        contact(a, SDL_MOUSEBUTTONUP,
                transform.logicalToPhysical({r.x + r.w * .5f, float(r.y + 30)}));
        REQUIRE(std::abs(a.settings_.brightness - .625f) < .01f);
        a.settings_.brightness = 1;
        a.applySettings();
        button(a, "Готово");
        REQUIRE(a.screen_ == App::Screen::Home);
        bounds(a);
        button(a, "Библиотека");
        a.library_.clear();
        for (int i = 0; i < 18; ++i) {
            Book b;
            b.path = "tests/fixtures/book.pdf";
            b.title = "Книга " + std::to_string(i);
            b.format = "pdf";
            b.author = "Тестовый автор";
            a.library_.push_back(b);
        }
        a.updateList();
        bounds(a);
        button(a, "Далее");
        REQUIRE(a.listStart_ > 0);
        button(a, "Назад");
        REQUIRE(a.listStart_ == 0);
        button(a, "Поиск");
        bounds(a);
        button(a, "ё");
        REQUIRE(a.query_ == "ё");
        button(a, "Поиск");
        REQUIRE(a.screen_ == App::Screen::Library);
        for (auto screen : {App::Screen::Recent, App::Screen::Favorites, App::Screen::Context,
                            App::Screen::Contents}) {
            a.setScreen(screen);
            bounds(a);
        }
        a.openBook("tests/fixtures/book.pdf");
        waitBook(a);
        frame(a);
        REQUIRE(a.view_.pages == 2);
        // Upward physical movement is a left swipe in the rotated UI.
        contact(a, SDL_MOUSEBUTTONDOWN, transform.logicalToPhysical({550, 600}));
        contact(a, SDL_MOUSEMOTION, transform.logicalToPhysical({250, 600}));
        contact(a, SDL_MOUSEBUTTONUP, transform.logicalToPhysical({250, 600}));
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        waitBook(a);
        REQUIRE(a.view_.position.page == 1);
        // Two physical contacts zoom the rotated PDF; a one-finger drag pans it.
        frame(a);
        finger(a, SDL_FINGERDOWN, 10, {280, 500});
        finger(a, SDL_FINGERDOWN, 11, {440, 500});
        finger(a, SDL_FINGERMOTION, 11, {600, 500});
        finger(a, SDL_FINGERUP, 11, {600, 500});
        finger(a, SDL_FINGERUP, 10, {280, 500});
        REQUIRE(a.zoom_ > 1.5f);
        contact(a, SDL_MOUSEBUTTONDOWN, transform.logicalToPhysical({360, 600}));
        contact(a, SDL_MOUSEMOTION, transform.logicalToPhysical({450, 600}));
        contact(a, SDL_MOUSEBUTTONUP, transform.logicalToPhysical({450, 600}));
        REQUIRE(a.panX_ > 0);
        a.zoomDeadline_ = 0;
        a.setZoom(1);
        a.zoomDeadline_ = 0;
        a.overlay_ = true;
        bounds(a);
        button(a, "Настройки");
        button(a, "Вертикальная");
        REQUIRE(a.width() == 1280);
        waitBook(a);
        REQUIRE(a.view_.position.page == 1); // PDF page survives presentation rotation.
        bounds(a);
        button(a, "Горизонтальная");
        waitBook(a);
        a.closeBook();
        a.openBook("tests/fixtures/book3.epub");
        waitBook(a);
        REQUIRE(!a.view_.fixed && a.view_.pages > 0);
        a.setScreen(App::Screen::Contents);
        bounds(a);
        REQUIRE(!a.view_.chapters.empty());
        auto c = a.ui_->buttons[1].rect;
        tap(a, {float(c.x + 40), float(c.y + 24)});
        REQUIRE(a.screen_ == App::Screen::Reader);
        if (a.view_.pages > 1) {
            a.navigate(a.view_.pages - 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            waitBook(a);
        }
        float progress = a.view_.position.progress;
        a.changeOrientation();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        waitBook(a);
        REQUIRE(a.view_.pages <= 1 || std::abs(a.view_.position.progress - progress) < .3f);
        a.changeOrientation();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        waitBook(a);
        frame(a);
        a.closeBook();
        a.savePosition(true);
        Config reloaded(std::string(temp) + "/config/state.ini");
        REQUIRE(reloaded.load());
        REQUIRE(reloaded.settings.language == Language::Russian &&
                reloaded.settings.orientation == 1);
        // Keep the public screenshots reproducible and entirely in English.
        a.config_.books.clear();
        Book samplePdf;
        samplePdf.path = "tests/fixtures/book.pdf";
        samplePdf.title = "Sample PDF";
        samplePdf.format = "pdf";
        samplePdf.opened = 2;
        samplePdf.position.progress = .42f;
        a.config_.books[samplePdf.path] = samplePdf;
        Book sampleEpub;
        sampleEpub.path = "tests/fixtures/book3.epub";
        sampleEpub.title = "Sample EPUB";
        sampleEpub.format = "epub";
        sampleEpub.opened = 1;
        sampleEpub.position.progress = .18f;
        a.config_.books[sampleEpub.path] = sampleEpub;
        a.settings_.language = Language::English;
        a.settings_.orientation = 1;
        a.setScreen(App::Screen::Home);
        waitIdle(a);
        bounds(a);
        capture(a, "screenshots/home-en-portrait.bmp");
        a.openBook("tests/fixtures/book.pdf");
        waitBook(a);
        a.settings_.language = Language::English;
        a.overlay_ = true;
        bounds(a);
        capture(a, "screenshots/reader-en-portrait.bmp");
        a.setScreen(App::Screen::Settings);
        bounds(a);
        capture(a, "screenshots/settings-en-portrait.bmp");
        a.message_ = "Invalid EPUB file";
        bounds(a);
        a.settings_.language = Language::Russian;
        bounds(a);
        REQUIRE(localizedError("Invalid EPUB file", Language::Russian) == "Повреждённый EPUB");
        a.message_.clear();
        std::cout << "PASS: " << checks
                  << " v2 UI checks (transformed touch, layout, Cyrillic, EPUB/PDF, persistence)\n";
    }
    static void restart() {
        App a;
        REQUIRE(a.run() == 0);
        REQUIRE(a.settings_.language == Language::Russian);
        REQUIRE(a.settings_.orientation == 1);
        bounds(a);
        std::cout << "PASS: saved Russian/portrait restored by fresh App instance\n";
    }
};
} // namespace br
int main() {
    try {
        br::AppTest::run();
        br::AppTest::restart();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
