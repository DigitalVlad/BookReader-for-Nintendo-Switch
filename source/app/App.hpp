#pragma once
#include "app/Worker.hpp"
#include "config/Config.hpp"
#include "ui/Components.hpp"
#include "ui/Gestures.hpp"
#include "ui/OrientationTransform.hpp"
#include <future>
#include <map>
#include <memory>
namespace br {
class WebUploadServer;
class App {
#ifndef __SWITCH__
    friend class AppTest;
#endif
    bool plInitialized_ = false, hookInstalled_ = false;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* canvas_ = nullptr;
    int canvasOrientation_ = -1;
    bool touchSuppressed_ = false;
    SDL_Texture* pageTexture_ = nullptr;
    SDL_Texture* previousTexture_ = nullptr;
    SDL_Texture* nextTexture_ = nullptr;
    std::unique_ptr<Components> ui_;
    std::unique_ptr<Worker> worker_;
    std::unique_ptr<WebUploadServer> web_;
    bool webActive_ = false, showWebHelp_ = false, rescanPending_ = false;
    std::vector<FontSource> fonts_;
    Config config_;
    Settings settings_;
    ReaderView view_;
    std::string activePath_, message_, query_;
    std::vector<Book> library_;
    std::future<std::vector<Book>> scan_;
    std::atomic<bool> cancelScan_{false};
    enum class Screen {
        Home,
        Library,
        Recent,
        Favorites,
        Reader,
        Settings,
        Contents,
        Search,
        Context
    };
    Screen screen_ = Screen::Home, returnScreen_ = Screen::Home;
    std::vector<std::string> filtered_;
    std::string contextPath_;
    int sort_ = 0, focus_ = 0, listStart_ = 0, tocStart_ = 0;
    bool running_ = true, busy_ = false, overlay_ = false, settingsDirty_ = false,
         keyboardRussian_ = false;
    float zoom_ = 1, panX_ = 0, panY_ = 0, inertia_ = 0, listDrag_ = 0;
    int slider_ = -1;
    Gestures gestures_;
    std::map<long long, TouchPoint> fingers_;
    std::vector<SDL_Joystick*> joysticks_;
    double lastCheckpoint_ = 0;
    double previousFrame_ = 0, lastInput_ = 0, zoomDeadline_ = 0, repeatAt_ = 0;
    uint64_t displayedRevision_ = 0;
    std::map<std::string, SDL_Texture*> covers_;
    Gesture pendingTap_;
    double tapDeadline_ = 0;
    int scrollTransition_ = 0;
    float scrollRemainder_ = 0;
    void draw();
    void drawLibrary();
    void drawReader();
    void drawSettings();
    void drawContents();
    void drawSearch();
    void drawContext();
    void drawWebStatus();
    void toggleWebUpload();
    void pollWebServer();
    void collectView();
    void savePosition(bool force);
    void updateList();
    void scanLibraryAsync();
    void openBook(const std::string& path);
    void closeBook();
    void navigate(int delta);
    void chapter(int direction);
    void setZoom(float zoom);
    void pan(float dx, float dy);
    void clampPan();
    void toggleFavorite(const std::string& path);
    void showSettings();
    void leaveSettings();
    void applySettings();
    void search();
    void back();
    void activate();
    void handleGesture(const Gesture& gesture);
    void input(double now, float dt);
    void key(SDL_Keycode key);
    void setSlider(int index, float x);
    void moveFocus(int delta);
    void setScreen(Screen screen);
    std::string tr(const std::string& key) const { return br::tr(key, settings_.language); }
    int width() const { return settings_.width(); }
    int height() const { return settings_.height(); }
    int listTop() const { return settings_.orientation ? 248 : 172; }
    int listRows() const { return (height() - listTop() - 96) / 88; }
    int tocRows() const { return (height() - 220) / 70; }
    SDL_Rect sliderRect(int index) const;
    void changeOrientation();
    void beginCanvas();
    void presentCanvas();

  public:
    App();
    ~App();
    int run();
    void flush() { savePosition(true); }
};
} // namespace br
