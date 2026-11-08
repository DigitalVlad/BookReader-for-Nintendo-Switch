#include "app/App.hpp"
#include "config/Config.hpp"
#include "utils/Files.hpp"
#include <SDL.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>

static void pause(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
static void key(SDL_Keycode code) {
    SDL_Event e{};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = code;
    SDL_PushEvent(&e);
    pause(100);
}
static void mouse(Uint32 type, int x, int y) {
    SDL_Event e{};
    e.type = type;
    if (type == SDL_MOUSEMOTION) {
        e.motion.x = x;
        e.motion.y = y;
    } else {
        e.button.button = SDL_BUTTON_LEFT;
        e.button.x = x;
        e.button.y = y;
    }
    SDL_PushEvent(&e);
    pause(100);
}
int main() {
    char dir[] = "/tmp/bookreader-gui-XXXXXX";
    if (!mkdtemp(dir))
        return 1;
    setenv("BOOKREADER_ROOT", dir, 1);
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("BOOKREADER_OPEN", "tests/fixtures/book.pdf", 1);
    std::thread driver([] {
        pause(1200);
        key(SDLK_x);
        key(SDLK_PLUS);
        mouse(SDL_MOUSEBUTTONDOWN, 900, 420); // brightness slider
        mouse(SDL_MOUSEBUTTONUP, 900, 420);
        key(SDLK_ESCAPE);
        key(SDLK_x); // back to page
        mouse(SDL_MOUSEBUTTONDOWN, 600, 350);
        mouse(SDL_MOUSEBUTTONUP, 600, 350);
        pause(20);
        mouse(SDL_MOUSEBUTTONDOWN, 600, 350);
        mouse(SDL_MOUSEBUTTONUP, 600, 350);
        // Two taps are spaced deliberately below the 300 ms double-tap threshold.
        pause(400);
        key(SDLK_RIGHT);
        pause(400);
        key(SDLK_y);
        pause(100);
        key(SDLK_q);
    });
    int result = 1;
    try {
        br::App app;
        result = app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    driver.join();
    if (result)
        return result;
    br::Config config(std::string(dir) + "/config/state.ini");
    if (!config.load())
        return 2;
    auto i = config.books.find("tests/fixtures/book.pdf");
    if (i == config.books.end() || i->second.position.page != 1 || i->second.position.zoom != 2 ||
        !i->second.favorite || i->second.settings.brightness >= .99f)
        return 3;
    std::cout << "PASS: GUI event loop, slider, navigation, favorite, exit persistence\n";
    return 0;
}
