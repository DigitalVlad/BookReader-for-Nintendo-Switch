#include "app/App.hpp"
#include "utils/Files.hpp"
#include <ctime>
namespace br {
void App::drawLibrary() {
    auto& u = *ui_;
    u.text(tr("BOOK READER"), 40, 22, 38, u.colors.text);
    if (screen_ == Screen::Home) {
        u.text(tr("Your library. Wherever you are."), 42, 72, 22, u.colors.muted, width() - 84);
        u.button({40, 134, 510, 64}, tr("Continue Reading"), [this] {
            if (!config_.lastBook.empty())
                openBook(config_.lastBook);
            else
                message_ = "Open a book from Library first";
        });
        u.button({40, 228, 245, 64}, tr("Library"), [this] {
            query_.clear();
            setScreen(Screen::Library);
            listStart_ = 0;
            updateList();
        });
        u.button({305, 228, 245, 64}, tr("Recent"), [this] {
            setScreen(Screen::Recent);
            listStart_ = 0;
            updateList();
        });
        u.button({40, 314, 245, 64}, tr("Favorites"), [this] {
            setScreen(Screen::Favorites);
            listStart_ = 0;
            updateList();
        });
        u.button({305, 314, 245, 64}, tr("Settings"), [this] { showSettings(); });
        u.button({40, height() - 90, 245, 60}, tr("Exit"), [this] { running_ = false; });
        int recentX = settings_.orientation ? 40 : 620, recentY = settings_.orientation ? 440 : 134;
        u.text(tr("RECENT BOOKS"), recentX, recentY, 24, u.colors.muted);
        std::vector<Book> recent;
        for (auto& e : config_.books)
            if (e.second.opened)
                recent.push_back(e.second);
        std::sort(recent.begin(), recent.end(),
                  [](auto& a, auto& b) { return a.opened > b.opened; });
        for (size_t i = 0; i < std::min(size_t(5), recent.size()); ++i) {
            auto b = recent[i];
            u.button({recentX, recentY + 50 + int(i) * 90, width() - recentX - 40, 72},
                     b.title + "  " + std::to_string(int(b.position.progress * 100)) + "%",
                     [this, b] { openBook(b.path); });
        }
        return;
    }
    const char* names[] = {"Title", "Author", "Date", "Format", "Last opened"};
    int colWidth = (width() - 80 - 24) / 3;
    auto nav = [&](int n) {
        return settings_.orientation
                   ? SDL_Rect{40 + (n % 3) * (colWidth + 12), 90 + (n / 3) * 70, colWidth, 56}
                   : SDL_Rect{40 + n * 200, 90, 188, 56};
    };
    u.button(nav(0), tr("Home"), [this] { setScreen(Screen::Home); });
    u.button(nav(1), tr("Search"), [this] { search(); });
    u.button(nav(2), tr(names[sort_]), [this] {
        sort_ = (sort_ + 1) % 5;
        updateList();
    });
    u.button(nav(3), tr("Refresh"), [this] { scanLibraryAsync(); });
    u.button(nav(4), tr("Recent"), [this] {
        setScreen(Screen::Recent);
        listStart_ = 0;
        updateList();
    });
    u.button(nav(5), tr("Favorites"), [this] {
        setScreen(Screen::Favorites);
        listStart_ = 0;
        updateList();
    });
    for (int row = 0; row < listRows() && listStart_ + row < int(filtered_.size()); ++row) {
        auto path = filtered_[listStart_ + row];
        auto i =
            std::find_if(library_.begin(), library_.end(), [&](auto& b) { return b.path == path; });
        if (i == library_.end())
            continue;
        auto b = *i;
        int y = listTop() + row * 88;
        u.button({40, y, width() - 248, 78}, "", [this, path] { openBook(path); });
        u.text(b.title, 122, y + 4, 25, u.colors.text, width() - 344);
        std::string meta = b.format + "  " + (b.author.empty() ? tr("Unknown author") : b.author) +
                           "  |  " + std::to_string(int(b.position.progress * 100)) + "%";
        if (b.opened) {
            std::time_t time = b.opened;
            char date[32]{};
            if (auto* tm = std::localtime(&time))
                strftime(date, sizeof(date), "%Y-%m-%d", tm);
            meta += "  |  ";
            meta += date;
        }
        u.text(meta, 122, y + 39, 18, u.colors.muted, width() - 344);
        auto cover = rootPath() + "/cache/covers/" + fingerprint(path) + ".bmp";
        auto entry = covers_.find(cover);
        if (entry == covers_.end()) {
            SDL_Texture* t = nullptr;
            if (auto* surface = SDL_LoadBMP(cover.c_str())) {
                t = SDL_CreateTextureFromSurface(renderer_, surface);
                SDL_FreeSurface(surface);
            }
            if (covers_.size() >= 24) {
                SDL_DestroyTexture(covers_.begin()->second);
                covers_.erase(covers_.begin());
            }
            entry = covers_.emplace(cover, t).first;
        }
        if (entry->second) {
            SDL_Rect d{52, y + 5, 48, 68};
            SDL_RenderCopy(renderer_, entry->second, nullptr, &d);
        } else {
            u.rect({54, y + 12, 42, 54}, u.colors.accent);
            u.text(b.format, 55, y + 26, 15, u.colors.text, 42);
        }
        u.button({width() - 192, y, 152, 78}, tr(b.favorite ? "Unstar" : "Star"),
                 [this, path] { toggleFavorite(path); });
        if (focus_ == 6 + row * 2 || focus_ == 7 + row * 2)
            contextPath_ = path;
    }
    if (filtered_.empty()) {
        u.text(tr(scan_.valid() ? "Scanning microSD..." : "No books"), 44, listTop() + 62, 26,
               u.colors.muted, width() - 88);
        u.text("/switch/BookReader/books/", 44, listTop() + 110, 24, u.colors.muted, width() - 88);
    }
    u.button({40, height() - 84, 160, 56}, tr("Previous"),
             [this] { listStart_ = std::max(0, listStart_ - listRows()); });
    u.button({218, height() - 84, 160, 56}, tr("Next"), [this] {
        listStart_ =
            std::min(std::max(0, int(filtered_.size()) - listRows()), listStart_ + listRows());
    });
    u.text(std::to_string(filtered_.size()) + " " + tr("books") + "  " + query_, 416, height() - 72,
           23, u.colors.muted, width() - 456);
}
void App::drawContext() {
    auto& u = *ui_;
    u.text(tr("BOOK OPTIONS"), 60, 110, 32, u.colors.text, width() - 120);
    u.text(basename(contextPath_), 60, 180, 26, u.colors.muted, width() - 120);
    u.button({60, 270, width() - 120, 70}, tr("Open"), [this] { openBook(contextPath_); });
    u.button({60, 365, width() - 120, 70}, tr("Toggle favorite"), [this] {
        toggleFavorite(contextPath_);
        setScreen(returnScreen_);
    });
    u.button({60, 460, width() - 120, 70}, tr("Back"), [this] { setScreen(returnScreen_); });
}
void App::drawSearch() {
    auto& u = *ui_;
    u.text(tr("SEARCH TITLE"), 40, 24, 32, u.colors.text, width() - 80);
    u.button({width() - 190, height() - 90, 150, 60}, tr("Back"), [this] { back(); });
    u.text(query_, 40, 83, 30, u.colors.accent, width() - 80);
    std::vector<std::string> letters;
    std::string alphabet =
        keyboardRussian_ ? "йцукенгшщзхъфывапролджэячсмитьбюё" : "qwertyuiopasdfghjklzxcvbnm";
    size_t at = 0;
    while (at < alphabet.size())
        letters.push_back(utf8(utf8Next(alphabet, at)));
    int columns = settings_.orientation ? 7 : 11, cell = (width() - 80) / columns;
    for (size_t i = 0; i < letters.size(); ++i) {
        auto letter = letters[i];
        u.button({40 + int(i % columns) * cell, 160 + int(i / columns) * 85, cell - 10, 68}, letter,
                 [this, letter] {
                     if (query_.size() < 240)
                         query_ += letter;
                 });
    }
    int controlY = settings_.orientation ? 640 : 470, controlCols = settings_.orientation ? 2 : 5;
    int controlW = (width() - 80) / controlCols;
    auto control = [&](int n) {
        return SDL_Rect{40 + (n % controlCols) * controlW, controlY + (n / controlCols) * 82,
                        controlW - 14, 64};
    };
    u.button(control(0), "EN / RU", [this] { keyboardRussian_ = !keyboardRussian_; });
    u.button(control(1), tr("Space"), [this] {
        if (query_.size() < 240)
            query_ += ' ';
    });
    u.button(control(2), tr("Backspace"), [this] {
        if (!query_.empty()) {
            size_t i = query_.size() - 1;
            while (i && ((query_[i] & 0xC0) == 0x80))
                --i;
            query_.erase(i);
        }
    });
    u.button(control(3), tr("Clear"), [this] { query_.clear(); });
    u.button(control(4), tr("Search"), [this] { back(); });
}
} // namespace br
