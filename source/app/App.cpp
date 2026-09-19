#include "app/App.hpp"
#include "storage/Library.hpp"
#include "utils/Files.hpp"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <stdexcept>
#ifdef __SWITCH__
#include <switch.h>
#endif
namespace br {
#ifdef __SWITCH__
static PadState pad;
static AppletHookCookie hook;
static bool saveRequested = false;
static void onApplet(AppletHookType type, void*) {
    if (type == AppletHookType_OnFocusState || type == AppletHookType_OnExitRequest)
        saveRequested = true;
}
#endif
App::App() : config_(rootPath() + "/config/state.ini") {
}
App::~App() {
    if (worker_)
        worker_->stop();
    cancelScan_ = true;
    if (scan_.valid())
        scan_.wait();
    ui_.reset();
    SDL_DestroyTexture(canvas_);
    SDL_DestroyTexture(pageTexture_);
    SDL_DestroyTexture(previousTexture_);
    SDL_DestroyTexture(nextTexture_);
    for (auto& c : covers_)
        SDL_DestroyTexture(c.second);
    for (auto* j : joysticks_)
        SDL_JoystickClose(j);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
#ifdef __SWITCH__
    if (hookInstalled_)
        appletUnhook(&hook);
    if (plInitialized_)
        plExit();
#endif
}
void App::scanLibraryAsync() {
    if (scan_.valid())
        return;
    for (auto& c : covers_)
        SDL_DestroyTexture(c.second);
    covers_.clear();
    cancelScan_ = false;
    auto dir = rootPath() + "/books";
    scan_ =
        std::async(std::launch::async, [this, dir] { return br::scanLibrary(dir, cancelScan_); });
}
int App::run() {
    if (!ensureDirectories(rootPath()))
        throw std::runtime_error("Cannot create BookReader folders");
    config_.load();
    settings_ = config_.settings;
    keyboardRussian_ = settings_.language == Language::Russian;
    fonts_ = discoverFonts(rootPath());
#ifdef __SWITCH__
    if (R_SUCCEEDED(plInitialize(PlServiceType_User))) {
        plInitialized_ = true;
        PlFontData data[PlSharedFontType_Total]{};
        s32 count = 0;
        u64 language = 0;
        if (R_SUCCEEDED(setInitialize())) {
            setGetSystemLanguage(&language);
            setExit();
        }
        if (R_SUCCEEDED(plGetSharedFont(language, data, PlSharedFontType_Total, &count)))
            for (int i = 0; i < count; ++i)
                fonts_.push_back(
                    {"", static_cast<const unsigned char*>(data[i].address), long(data[i].size)});
        for (int type = 0; type < PlSharedFontType_NintendoExt; ++type) {
            PlFontData font{};
            if (R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType(type)))) {
                bool exists = std::any_of(fonts_.begin(), fonts_.end(),
                                          [&](const auto& s) { return s.data == font.address; });
                if (!exists)
                    fonts_.push_back(
                        {"", static_cast<const unsigned char*>(font.address), long(font.size)});
            }
        }
    }
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();
    appletHook(&hook, onApplet, nullptr);
    hookInstalled_ = true;
#else
    for (auto p : {"/System/Library/Fonts/Supplemental/Arial.ttf",
                   "/System/Library/Fonts/STHeiti Medium.ttc",
                   "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"})
        fonts_.push_back({p, nullptr, 0});
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0)
        throw std::runtime_error(SDL_GetError());
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    window_ = SDL_CreateWindow("BookReader", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280,
                               720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window_)
        throw std::runtime_error(SDL_GetError());
    renderer_ = SDL_CreateRenderer(window_, -1,
                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC |
                                       SDL_RENDERER_TARGETTEXTURE);
    if (!renderer_)
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer_)
        throw std::runtime_error(SDL_GetError());
    SDL_RenderSetLogicalSize(renderer_, 1280, 720);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (auto* j = SDL_JoystickOpen(i))
            joysticks_.push_back(j);
    }
    ui_ = std::make_unique<Components>(renderer_, fonts_);
    {
        Fonts check(fonts_);
        for (uint32_t c = 0x410; c <= 0x44f; ++c)
            if (!check.hasGlyph(c))
                throw std::runtime_error("No Cyrillic font. Add a TTF to fonts/");
        if (!check.hasGlyph(0x401) || !check.hasGlyph(0x451))
            throw std::runtime_error("No Cyrillic font. Add a TTF to fonts/");
    }
    worker_ = std::make_unique<Worker>(fonts_);
    scanLibraryAsync();
    previousFrame_ = monotonic();
    const char* autoOpen = std::getenv("BOOKREADER_OPEN");
    if (autoOpen)
        openBook(autoOpen);
    int frames = 0;
    const char* smoke = std::getenv("BOOKREADER_SMOKE_FRAMES");
    const int maxFrames = smoke ? std::atoi(smoke) : 0;
    while (running_
#ifdef __SWITCH__
           && appletMainLoop()
#endif
    ) {
        double now = monotonic();
        float dt = float(std::min(.05, now - previousFrame_));
        previousFrame_ = now;
#ifdef __SWITCH__
        if (saveRequested) {
            savePosition(true);
            saveRequested = false;
        }
        int desiredWidth = appletGetOperationMode() == AppletOperationMode_Console ? 1920 : 1280;
        int actualWidth = 0, actualHeight = 0;
        SDL_GetWindowSize(window_, &actualWidth, &actualHeight);
        if (actualWidth != desiredWidth)
            SDL_SetWindowSize(window_, desiredWidth, desiredWidth == 1920 ? 1080 : 720);
#endif
        if (scan_.valid() && scan_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                library_ = scan_.get();
                config_.merge(library_);
                updateList();
            } catch (const std::exception& e) {
                message_ = e.what();
            }
        }
        collectView();
        input(now, dt);
        if (std::abs(inertia_) > 8) {
            pan(0, inertia_ * dt);
            inertia_ *= std::exp(-6 * dt);
        } else
            inertia_ = 0;
        if (tapDeadline_ && now >= tapDeadline_) {
            tapDeadline_ = 0;
            auto g = pendingTap_;
            g.type = GestureType::None;
            if (screen_ == Screen::Reader && !overlay_) {
                if (g.x < width() * .25f)
                    navigate(-1);
                else if (g.x > width() * .75f)
                    navigate(1);
                else {
                    overlay_ = true;
                    focus_ = 0;
                }
            }
        }
        if (zoomDeadline_ && now >= zoomDeadline_) {
            Request r;
            r.kind = Request::Zoom;
            r.zoom = zoom_;
            worker_->submit(r);
            zoomDeadline_ = 0;
            savePosition(false);
        }
        if (now - lastCheckpoint_ >= 15) {
            savePosition(false);
            lastCheckpoint_ = now;
        }
        if (!config_.save(now))
            message_ = "Cannot save progress: check microSD space";
        draw();
#ifndef __SWITCH__
        if (maxFrames && frames + 1 >= maxFrames) {
            if (const char* p = std::getenv("BOOKREADER_SCREENSHOT")) {
                SDL_Surface* surface =
                    SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA32);
                if (surface) {
                    if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGBA32,
                                             surface->pixels, surface->pitch) == 0)
                        SDL_SaveBMP(surface, p);
                    SDL_FreeSurface(surface);
                }
            }
        }
#endif
        SDL_RenderPresent(renderer_);
        if (maxFrames && ++frames >= maxFrames)
            running_ = false;
        SDL_Delay(8);
    }
    savePosition(true);
    return 0;
}
void App::setScreen(Screen screen) {
    tapDeadline_ = 0;
    screen_ = screen;
    focus_ = 0;
    inertia_ = 0;
    listDrag_ = 0;
}
void App::updateList() {
    sortLibrary(library_, screen_ == Screen::Recent ? 4 : sort_);
    filtered_.clear();
    for (const auto& b : library_) {
        if (screen_ == Screen::Favorites && !b.favorite)
            continue;
        if (screen_ == Screen::Recent && !b.opened)
            continue;
        if (!query_.empty() && fold(b.title).find(fold(query_)) == std::string::npos)
            continue;
        filtered_.push_back(b.path);
        if (screen_ == Screen::Recent && filtered_.size() == 20)
            break;
    }
    listStart_ = std::clamp(listStart_, 0, std::max(0, int(filtered_.size()) - listRows()));
}
void App::openBook(const std::string& p) {
    savePosition(true);
    activePath_ = p;
    auto it = config_.books.find(p);
    Book b;
    if (it != config_.books.end())
        b = it->second;
    else {
        auto f =
            std::find_if(library_.begin(), library_.end(), [&](auto& x) { return x.path == p; });
        if (f != library_.end())
            b = *f;
    }
    settings_ = b.hasSettings ? b.settings : config_.settings;
    settings_.language = config_.settings.language;
    settings_.orientation = config_.settings.orientation;
    zoom_ = b.position.zoom;
    panX_ = b.position.panX;
    panY_ = b.position.panY;
    view_ = {};
    view_.path = p;
    SDL_DestroyTexture(pageTexture_);
    pageTexture_ = nullptr;
    SDL_DestroyTexture(previousTexture_);
    previousTexture_ = nullptr;
    SDL_DestroyTexture(nextTexture_);
    nextTexture_ = nullptr;
    setScreen(Screen::Reader);
    overlay_ = false;
    message_.clear();
    Request r;
    r.kind = Request::Open;
    r.path = p;
    r.settings = settings_;
    r.position = b.position;
    worker_->submit(r);
    busy_ = true;
}
void App::closeBook() {
    savePosition(true);
    Request r;
    r.kind = Request::Close;
    worker_->submit(r);
    activePath_.clear();
    view_ = {};
    SDL_DestroyTexture(pageTexture_);
    pageTexture_ = nullptr;
    SDL_DestroyTexture(previousTexture_);
    previousTexture_ = nullptr;
    SDL_DestroyTexture(nextTexture_);
    nextTexture_ = nullptr;
    settings_ = config_.settings;
    setScreen(Screen::Library);
    config_.merge(library_);
    for (auto& c : covers_)
        SDL_DestroyTexture(c.second);
    covers_.clear();
    updateList();
}
void App::collectView() {
    auto snapshot = worker_->snapshot(busy_, displayedRevision_);
    if (snapshot.revision == displayedRevision_)
        return;
    displayedRevision_ = snapshot.revision;
    if (snapshot.path != activePath_)
        return;
    if (!snapshot.error.empty()) {
        message_ = snapshot.error;
        if (snapshot.pages == 0)
            overlay_ = true;
        return;
    }
    if (!snapshot.bitmap)
        return;
    bool firstOpen = view_.pages == 0;
    bool changed = firstOpen || snapshot.position.page != view_.position.page;
    view_ = std::move(snapshot);
    if (changed && view_.position.page != config_.books[activePath_].position.page) {
        panX_ = 0;
        panY_ = scrollTransition_ ? scrollRemainder_ : 0;
        if (!scrollTransition_)
            inertia_ = 0;
        scrollTransition_ = 0;
    }
    SDL_DestroyTexture(pageTexture_);
    pageTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                     view_.bitmap->width, view_.bitmap->height);
    if (pageTexture_)
        SDL_UpdateTexture(pageTexture_, nullptr, view_.bitmap->rgba.data(),
                          view_.bitmap->width * 4);
    else
        message_ = "Not enough graphics memory";
    auto upload = [this](SDL_Texture*& texture, const std::shared_ptr<Bitmap>& b) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
        if (b) {
            texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                        b->width, b->height);
            if (texture)
                SDL_UpdateTexture(texture, nullptr, b->rgba.data(), b->width * 4);
        }
    };
    upload(previousTexture_, view_.previous);
    upload(nextTexture_, view_.next);
    clampPan();
    auto& b = config_.books[activePath_];
    b.path = activePath_;
    b.title = view_.title;
    b.author = view_.author;
    b.format = extension(activePath_);
    if (firstOpen)
        b.opened = std::time(nullptr);
    config_.lastBook = activePath_;
    config_.changed(monotonic());
    savePosition(false);
}
void App::savePosition(bool force) {
    if (!activePath_.empty() && view_.pages > 0) {
        auto& b = config_.books[activePath_];
        bool changed = !b.hasSettings || b.position.page != view_.position.page ||
                       b.position.offset != view_.position.offset || b.position.zoom != zoom_ ||
                       b.position.panX != panX_ || b.position.panY != panY_ ||
                       !(b.settings == settings_);
        b.path = activePath_;
        b.position = view_.position;
        b.position.zoom = zoom_;
        b.position.panX = panX_;
        b.position.panY = panY_;
        b.settings = settings_;
        b.hasSettings = true;
        if (changed)
            config_.changed(monotonic());
    }
    if (force && !config_.save(monotonic(), true))
        message_ = "Cannot save progress: check microSD space";
}
void App::navigate(int delta) {
    if (activePath_.empty() || !view_.pages)
        return;
    Request r;
    r.kind = Request::Page;
    r.delta = delta;
    worker_->submit(r);
    panX_ = panY_ = inertia_ = 0;
}
void App::chapter(int direction) {
    if (view_.chapters.empty()) {
        navigate(direction);
        return;
    }
    int p = view_.position.page, target = direction > 0 ? view_.pages - 1 : 0;
    if (direction > 0) {
        for (auto& c : view_.chapters)
            if (c.page > p) {
                target = c.page;
                break;
            }
    } else {
        for (auto& c : view_.chapters)
            if (c.page < p)
                target = std::max(target, c.page);
    }
    Request r;
    r.kind = Request::Jump;
    r.delta = target;
    worker_->submit(r);
    panX_ = panY_ = 0;
}
void App::setZoom(float z) {
    if (!view_.fixed)
        return;
    zoom_ = std::clamp(z, 1.f, 4.f);
    clampPan();
    zoomDeadline_ = monotonic() + .15;
}
void App::clampPan() {
    float boundX = settings_.width() * (zoom_ - 1) / 2,
          boundY = settings_.height() * (zoom_ - 1) / 2;
    if (view_.fixed && view_.bitmap) {
        float scale = std::min(float(settings_.width()) / view_.bitmap->width,
                               float(settings_.height()) / view_.bitmap->height) *
                      zoom_;
        boundX = std::max(0.f, (view_.bitmap->width * scale - settings_.width()) / 2);
        boundY = std::max(0.f, (view_.bitmap->height * scale - settings_.height()) / 2);
    }
    if (!view_.fixed) {
        boundX = 0;
        boundY = settings_.scrollMode ? float(settings_.height()) : 0;
    }
    panX_ = std::clamp(panX_, -boundX, boundX);
    panY_ = std::clamp(panY_, -boundY, boundY);
    if (!view_.fixed) {
        if (view_.position.page == 0)
            panY_ = std::min(0.f, panY_);
        if (view_.position.page + 1 == view_.pages)
            panY_ = std::max(0.f, panY_);
    }
}
void App::pan(float dx, float dy) {
    if (screen_ != Screen::Reader || overlay_)
        return;
    panX_ += dx;
    panY_ += dy;
    if (!view_.fixed && settings_.scrollMode && !scrollTransition_ && !busy_) {
        int delta = panY_ <= -settings_.height() ? 1 : panY_ > 0 ? -1 : 0;
        if (delta && view_.position.page + delta >= 0 &&
            view_.position.page + delta < view_.pages) {
            scrollTransition_ = delta;
            scrollRemainder_ = panY_ + delta * settings_.height();
            Request r;
            r.kind = Request::Page;
            r.delta = delta;
            worker_->submit(r);
        }
    }
    clampPan();
}
void App::toggleFavorite(const std::string& p) {
    if (p.empty())
        return;
    auto& b = config_.books[p];
    if (b.path.empty()) {
        auto i =
            std::find_if(library_.begin(), library_.end(), [&](auto& b) { return b.path == p; });
        if (i != library_.end())
            b = *i;
        b.path = p;
    }
    b.favorite = !b.favorite;
    config_.changed(monotonic());
    config_.merge(library_);
    updateList();
}
void App::showSettings() {
    if (screen_ == Screen::Settings) {
        leaveSettings();
        return;
    }
    returnScreen_ = screen_;
    setScreen(Screen::Settings);
    settingsDirty_ = false;
}
void App::applySettings() {
    settings_.clamp();
    config_.settings.language = settings_.language;
    config_.settings.orientation = settings_.orientation;
    config_.changed(monotonic());
    if (!activePath_.empty()) {
        Request r;
        r.kind = Request::Configure;
        r.settings = settings_;
        worker_->submit(r);
        savePosition(false);
    } else {
        config_.settings = settings_;
        config_.changed(monotonic());
    }
    settingsDirty_ = false;
}
void App::leaveSettings() {
    if (settingsDirty_)
        applySettings();
    setScreen(returnScreen_);
}
void App::search() {
    returnScreen_ = screen_;
    keyboardRussian_ = settings_.language == Language::Russian;
    setScreen(Screen::Search);
    SDL_StartTextInput();
}
void App::back() {
    message_.clear();
    if (screen_ == Screen::Settings) {
        leaveSettings();
        return;
    }
    if (screen_ == Screen::Search) {
        SDL_StopTextInput();
        setScreen(returnScreen_);
        listStart_ = 0;
        updateList();
        return;
    }
    if (screen_ == Screen::Reader) {
        if (overlay_)
            overlay_ = false;
        else
            closeBook();
        return;
    }
    if (screen_ == Screen::Contents) {
        setScreen(Screen::Reader);
        overlay_ = true;
        return;
    }
    if (screen_ == Screen::Context) {
        setScreen(returnScreen_);
        return;
    }
    setScreen(Screen::Home);
}
void App::moveFocus(int d) {
    if (ui_->buttons.empty())
        return;
    focus_ = (focus_ + d + int(ui_->buttons.size())) % int(ui_->buttons.size());
}
void App::activate() {
    if (screen_ == Screen::Reader && !overlay_) {
        overlay_ = true;
        focus_ = 0;
        return;
    }
    if (!ui_->buttons.empty()) {
        focus_ = std::clamp(focus_, 0, int(ui_->buttons.size()) - 1);
        auto action = ui_->buttons[focus_].action;
        action();
    }
}
void App::key(SDL_Keycode k) {
    if (k == SDLK_ESCAPE) {
        back();
        return;
    }
    if (k == SDLK_RETURN) {
        activate();
        return;
    }
    if (screen_ == Screen::Search && k != SDLK_UP && k != SDLK_DOWN && k != SDLK_LEFT &&
        k != SDLK_RIGHT)
        return;
    if (k == SDLK_q) {
        running_ = false;
        return;
    }
    if (k == SDLK_x && screen_ == Screen::Reader) {
        overlay_ = !overlay_;
        focus_ = 0;
        return;
    }
    if (k == SDLK_MINUS) {
        showSettings();
        return;
    }
    if (k == SDLK_y) {
        toggleFavorite(screen_ == Screen::Reader ? activePath_ : contextPath_);
        return;
    }
    if (screen_ == Screen::Reader && !overlay_) {
        if (k == SDLK_RIGHT)
            navigate(1);
        if (k == SDLK_LEFT)
            navigate(-1);
        if (k == SDLK_UP)
            pan(0, 80);
        if (k == SDLK_DOWN)
            pan(0, -80);
        if (k == SDLK_PAGEUP)
            chapter(-1);
        if (k == SDLK_PAGEDOWN)
            chapter(1);
        return;
    }
    if (k == SDLK_DOWN || k == SDLK_RIGHT)
        moveFocus(1);
    if (k == SDLK_UP || k == SDLK_LEFT)
        moveFocus(-1);
}
void App::input(double now, float dt) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
            running_ = false;
        if (e.type == SDL_KEYDOWN && !e.key.repeat)
            key(e.key.keysym.sym);
        if (screen_ == Screen::Search && e.type == SDL_TEXTINPUT && query_.size() < 240)
            query_ += e.text.text;
        if (screen_ == Screen::Search && e.type == SDL_KEYDOWN &&
            e.key.keysym.sym == SDLK_BACKSPACE) {
            if (!query_.empty()) {
                size_t i = query_.size() - 1;
                while (i && ((query_[i] & 0xC0) == 0x80))
                    --i;
                query_.erase(i);
            }
        }
#ifndef __SWITCH__
        if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION)
            fingers_[e.tfinger.fingerId] = {e.tfinger.fingerId, e.tfinger.x * 1280,
                                            e.tfinger.y * 720};
        if (e.type == SDL_FINGERUP)
            fingers_.erase(e.tfinger.fingerId);
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.which != SDL_TOUCH_MOUSEID &&
            e.button.button == SDL_BUTTON_LEFT)
            fingers_[-1] = {-1, float(e.button.x), float(e.button.y)};
        if (e.type == SDL_MOUSEMOTION && fingers_.count(-1))
            fingers_[-1] = {-1, float(e.motion.x), float(e.motion.y)};
        if (e.type == SDL_MOUSEBUTTONUP && e.button.which != SDL_TOUCH_MOUSEID)
            fingers_.erase(-1);
        if (e.type == SDL_MOUSEWHEEL) {
            if (screen_ == Screen::Reader) {
                if (SDL_GetModState() & KMOD_CTRL)
                    setZoom(zoom_ + e.wheel.y * .2f);
                else
                    pan(0, e.wheel.y * 60.f);
            } else {
                listStart_ = std::clamp(listStart_ - e.wheel.y, 0,
                                        std::max(0, int(filtered_.size()) - listRows()));
            }
        }
#endif
    }
    std::vector<TouchPoint> touches;
#ifdef __SWITCH__
    padUpdate(&pad);
    u64 down = padGetButtonsDown(&pad), held = padGetButtons(&pad);
    if (down & HidNpadButton_A)
        activate();
    if (down & HidNpadButton_B)
        back();
    if (down & HidNpadButton_X && screen_ == Screen::Reader) {
        overlay_ = !overlay_;
        focus_ = 0;
    }
    if (down & HidNpadButton_Y)
        toggleFavorite(screen_ == Screen::Reader ? activePath_ : contextPath_);
    if (down & HidNpadButton_Minus)
        showSettings();
    if (down & HidNpadButton_Plus)
        showSettings();
    u64 directions =
        HidNpadButton_Left | HidNpadButton_Right | HidNpadButton_Up | HidNpadButton_Down;
    u64 fire = down;
    if (!(held & directions))
        repeatAt_ = 0;
    else if (down & directions)
        repeatAt_ = now + .35;
    else if (now >= repeatAt_) {
        fire |= held & directions;
        repeatAt_ = now + .12;
    }
    if (settings_.orientation) {
        u64 original = fire;
        fire &= ~directions;
        if (original & HidNpadButton_Left)
            fire |= HidNpadButton_Down;
        if (original & HidNpadButton_Right)
            fire |= HidNpadButton_Up;
        if (original & HidNpadButton_Up)
            fire |= HidNpadButton_Left;
        if (original & HidNpadButton_Down)
            fire |= HidNpadButton_Right;
    }
    if (screen_ == Screen::Reader && !overlay_) {
        if (fire & HidNpadButton_Left)
            navigate(-1);
        if (fire & HidNpadButton_Right)
            navigate(1);
        if (fire & HidNpadButton_Up)
            pan(0, 80);
        if (fire & HidNpadButton_Down)
            pan(0, -80);
        if (down & HidNpadButton_L)
            chapter(-1);
        if (down & HidNpadButton_R)
            chapter(1);
        if (down & HidNpadButton_ZL)
            navigate(-10);
        if (down & HidNpadButton_ZR)
            navigate(10);
        for (int i = 0; i < 2; ++i) {
            auto a = padGetStickPos(&pad, i);
            if (std::abs(a.x) > 6000 || std::abs(a.y) > 6000) {
                auto v = OrientationTransform(settings_.orientation)
                             .physicalVectorToLogical(
                                 {-a.x / 32768.f * 700 * dt, a.y / 32768.f * 700 * dt});
                pan(v.x, v.y);
            }
        }
    } else {
        if (fire & (HidNpadButton_Down | HidNpadButton_Right))
            moveFocus(1);
        if (fire & (HidNpadButton_Up | HidNpadButton_Left))
            moveFocus(-1);
    }
    HidTouchScreenState touch{};
    if (hidGetTouchScreenStates(&touch, 1))
        for (s32 i = 0; i < touch.count; ++i)
            touches.push_back(
                {touch.touches[i].finger_id, float(touch.touches[i].x), float(touch.touches[i].y)});
#else
    (void)dt;
    for (auto& p : fingers_)
        touches.push_back(p.second);
#endif
    if (touchSuppressed_) {
        if (touches.empty())
            touchSuppressed_ = false;
        return;
    }
    for (auto& p : touches) {
        auto logical = OrientationTransform(settings_.orientation).physicalToLogical({p.x, p.y});
        p.x = logical.x;
        p.y = logical.y;
    }
    bool drag =
        screen_ == Screen::Settings || (screen_ == Screen::Reader && view_.fixed && zoom_ > 1.05f);
    auto old = gestures_.state();
    auto g = gestures_.update(touches, now, drag);
    if (old == TouchState::Idle && !touches.empty()) {
        inertia_ = 0;
        slider_ = -1;
        if (screen_ == Screen::Settings) {
            auto p = touches.front();
            for (int i = 0; i < 4; ++i) {
                auto r = sliderRect(i);
                if (p.x >= r.x - 12 && p.x <= r.x + r.w + 12 && p.y >= r.y && p.y < r.y + r.h) {
                    slider_ = i;
                    setSlider(i, p.x);
                }
            }
        }
    }
    if (slider_ >= 0) {
        if (!touches.empty())
            setSlider(slider_, touches[0].x);
        else {
            slider_ = -1;
            if (settingsDirty_)
                applySettings();
        }
        return;
    }
    handleGesture(g);
}
void App::handleGesture(const Gesture& g) {
    if (g.type == GestureType::None)
        return;
    if (g.type == GestureType::Tap || g.type == GestureType::DoubleTap) {
        if (ui_->tap(g.x, g.y))
            return;
        if (screen_ == Screen::Reader && overlay_ && g.x >= width() * .25f &&
            g.x <= width() * .75f) {
            overlay_ = false;
            return;
        }
        if (screen_ == Screen::Reader && !overlay_ && view_.fixed) {
            if (g.type == GestureType::DoubleTap) {
                tapDeadline_ = 0;
                setZoom(zoom_ > 1.1f ? 1 : 2);
            } else {
                pendingTap_ = g;
                tapDeadline_ = monotonic() + .3;
            }
            return;
        }
        if (screen_ == Screen::Reader && !overlay_) {
            if (g.type == GestureType::DoubleTap && view_.fixed) {
                setZoom(zoom_ > 1.1f ? 1 : 2);
                return;
            }
            if (g.x < width() * .25f)
                navigate(-1);
            else if (g.x > width() * .75f)
                navigate(1);
            else {
                overlay_ = true;
                focus_ = 0;
            }
        }
        return;
    }
    if (g.type == GestureType::LongPress &&
        (screen_ == Screen::Library || screen_ == Screen::Recent || screen_ == Screen::Favorites)) {
        int row = int((g.y - listTop()) / 88);
        if (g.x >= 40 && g.x < width() - 208 && g.y >= listTop() && row >= 0 && row < listRows() &&
            listStart_ + row < int(filtered_.size())) {
            contextPath_ = filtered_[listStart_ + row];
            returnScreen_ = screen_;
            setScreen(Screen::Context);
        }
        return;
    }
    if (screen_ == Screen::Reader && !overlay_) {
        if (g.type == GestureType::Swipe)
            navigate(g.dx < 0 ? 1 : -1);
        if (g.type == GestureType::Drag)
            pan(g.dx, g.dy);
        if (g.type == GestureType::Pinch)
            setZoom(zoom_ * g.scale);
        if (g.type == GestureType::Release) {
            if (!view_.fixed && !settings_.scrollMode && std::abs(g.dy) > 60)
                navigate(g.dy < 0 ? 1 : -1);
            if (!view_.fixed && settings_.scrollMode)
                inertia_ = g.velocity;
            savePosition(false);
        }
    } else if (g.type == GestureType::Drag) {
        listDrag_ += g.dy;
        int rows = int(listDrag_ / 70);
        if (rows) {
            if (screen_ == Screen::Contents)
                tocStart_ = std::clamp(tocStart_ - rows, 0,
                                       std::max(0, int(view_.chapters.size()) - tocRows()));
            else
                listStart_ = std::clamp(listStart_ - rows, 0,
                                        std::max(0, int(filtered_.size()) - listRows()));
            listDrag_ -= rows * 70;
        }
    }
}
void App::draw() {
    beginCanvas();
    ui_->begin(settings_.theme);
    switch (screen_) {
    case Screen::Reader:
        drawReader();
        break;
    case Screen::Settings:
        drawSettings();
        break;
    case Screen::Contents:
        drawContents();
        break;
    case Screen::Search:
        drawSearch();
        break;
    case Screen::Context:
        drawContext();
        break;
    default:
        drawLibrary();
    }
    if (screen_ != Screen::Reader || overlay_)
        ui_->focus(focus_);
    if (busy_) {
        ui_->rect({width() - 230, 8, 220, 48}, ui_->colors.panel);
        ui_->text(tr("Loading..."), width() - 216, 15, 24, ui_->colors.text, 200);
    }
    if (!message_.empty()) {
        ui_->buttons.clear();
        ui_->rect({24, height() - 180, width() - 48, 160}, ui_->colors.panel);
        ui_->text(localizedError(message_, settings_.language), 40, height() - 168, 20,
                  ui_->colors.text, width() - 80);
        ui_->button({width() - 180, height() - 90, 120, 56}, tr("OK"),
                    [this] { message_.clear(); });
    }
    if (settings_.brightness < .999f)
        ui_->rect({0, 0, width(), height()}, {0, 0, 0, uint8_t((1 - settings_.brightness) * 255)});
    presentCanvas();
}
void App::beginCanvas() {
    if (canvasOrientation_ != settings_.orientation) {
        SDL_SetRenderTarget(renderer_, nullptr);
        SDL_DestroyTexture(canvas_);
        canvas_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                    width(), height());
        if (!canvas_)
            throw std::runtime_error(SDL_GetError());
        SDL_SetTextureBlendMode(canvas_, SDL_BLENDMODE_NONE);
        canvasOrientation_ = settings_.orientation;
    }
    SDL_SetRenderTarget(renderer_, canvas_);
    SDL_RenderSetLogicalSize(renderer_, width(), height());
    SDL_RenderSetViewport(renderer_, nullptr);
}
void App::presentCanvas() {
    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_RenderSetLogicalSize(renderer_, 1280, 720);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    OrientationTransform transform(settings_.orientation);
    auto rect = transform.presentationRect();
    SDL_RenderCopyEx(renderer_, canvas_, nullptr, &rect, transform.angle(), nullptr, SDL_FLIP_NONE);
}
} // namespace br
