#include "app/Worker.hpp"
#include "config/Config.hpp"
#include "readers/BookFactory.hpp"
#include "storage/Library.hpp"
#include "ui/Gestures.hpp"
#include "utils/Files.hpp"
#include "utils/MemoryManager.hpp"
#include <SDL.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
using namespace br;
static int assertions = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++assertions;                                                                              \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string("Check failed: ") + #x + " at " +                 \
                                     std::to_string(__LINE__));                                    \
    } while (0)
int main() {
    try {
        char temp[] = "/tmp/bookreader-test-XXXXXX";
        auto* directory = mkdtemp(temp);
        CHECK(directory);
        setenv("BOOKREADER_ROOT", directory, 1);
        CHECK(ensureDirectories(rootPath()));
        std::vector<FontSource> fonts = {
            {"/System/Library/Fonts/Supplemental/Arial.ttf", nullptr, 0},
            {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", nullptr, 0}};
        std::atomic<bool> cancel{false};
        Settings settings;
        std::string fixture = "tests/fixtures/";
        CHECK(escape("a%\n=b") == "a%25%0A%3Db");
        CHECK(unescape(escape("Книга\n[1]=%")) == "Книга\n[1]=%");
        CHECK(fold("ЁЖИК") == "ёжик");
        for (uint32_t c : {0x41u, 0x42fu, 0x4E2Du, 0x1F642u}) {
            size_t at = 0;
            CHECK(utf8Next(utf8(c), at) == c);
        }
        {
            MemoryManager memory(100);
            void* p = MemoryManager::allocate(&memory, 80);
            CHECK(p);
            CHECK(!MemoryManager::allocate(&memory, 30));
            CHECK(!MemoryManager::resize(&memory, p, 101));
            MemoryManager::release(&memory, p);
            CHECK(memory.used() == 0);
        }
        int pages = 0;
        uint64_t offset = 0;
        for (auto name : {"utf8.txt", "utf16le.txt", "utf16be.txt"}) {
            auto r = BookFactory::open(fixture + name, settings, fonts, cancel);
            CHECK(r->getPageCount() > 3);
            if (!pages)
                pages = r->getPageCount();
            CHECK(r->getPageCount() == pages);
            r->goToPage(2);
            auto pos = r->position();
            CHECK(pos.page == 2 && pos.offset > 0);
            offset = pos.offset;
            auto bitmap = r->render(1);
            CHECK(bitmap->width == 1280 && bitmap->height == 720);
            r->close();
            CHECK(r->open(fixture + name));
            r->restore(pos);
            CHECK(r->getCurrentPage() == 2);
            auto s = settings;
            s.fontSize = 38;
            r->configure(s);
            CHECK(r->position().offset <= offset);
            r->goToPage(1000000);
            CHECK(r->getCurrentPage() == r->getPageCount() - 1);
        }
        for (auto name : {"empty.txt", "invalid.txt"}) {
            auto r = BookFactory::open(fixture + name, settings, fonts, cancel);
            CHECK(r->getPageCount() == 1);
            CHECK(r->render(1)->rgba.size() > 0);
        }
        for (auto name :
             {"book.pdf", "book2.epub", "book3.epub", "book.html", "book.fb2", "comic.cbz"}) {
            std::cout << "Opening " << name << std::endl;
            auto r = BookFactory::open(fixture + name, settings, fonts, cancel);
            CHECK(r->getPageCount() > 0);
            auto bitmap = r->render(1);
            CHECK(bitmap->width > 0 && bitmap->height > 0);
            if (extension(name) == "pdf") {
                CHECK(r->getPageCount() == 2);
                auto zoomed = r->render(4);
                CHECK(zoomed->width <= 2048 && zoomed->height <= 2048);
                CHECK(zoomed->rgba.size() <= 4 * 1024 * 1024 * 4);
            }
            if (extension(name) == "epub") {
                CHECK(r->contents().size() == 2);
                CHECK(r->getAuthor() == "Test Author");
                CHECK(r->cover()->rgba.size() > 0);
            }
            if (extension(name) == "fb2") {
                CHECK(r->getTitle() == "Русская FB2");
                CHECK(r->getAuthor().find("Иван") != std::string::npos);
                CHECK(r->contents().size() >= 2);
            }
            if (!r->fixedLayout()) {
                auto s = settings;
                s.fontSize = 40;
                s.theme = 2;
                r->configure(s);
                auto dark = r->render(1);
                CHECK(dark->rgba.size() > 0);
                CHECK(dark->rgba[0] < 40 && dark->rgba[1] < 40 && dark->rgba[2] < 40);
            }
            r->nextPage();
            auto p = r->position();
            r->close();
            CHECK(r->open(fixture + name));
            r->restore(p);
            CHECK(r->getCurrentPage() == p.page);
        }
        for (auto name :
             {"invalid.pdf", "invalid.epub", "invalid.fb2", "missing.txt", "unsupported.azw"}) {
            bool threw = false;
            try {
                auto r = BookFactory::open(fixture + name, settings, fonts, cancel);
            } catch (const std::exception&) {
                threw = true;
            }
            CHECK(threw);
        }
        {
            Config c(rootPath() + "/config/state.ini");
            Book b;
            b.path = "sdmc:/switch/BookReader/books/中文=100%.epub";
            b.title = "Русская книга";
            b.favorite = true;
            b.position.offset = 987654321;
            b.position.page = 42;
            b.position.zoom = 2.5;
            c.books[b.path] = b;
            c.lastBook = b.path;
            c.changed(1);
            CHECK(c.save(2, true));
            Config restored(rootPath() + "/config/state.ini");
            CHECK(restored.load());
            CHECK(restored.books.at(b.path).title == b.title);
            CHECK(restored.books.at(b.path).position.offset == 987654321);
            CHECK(restored.books.at(b.path).favorite);
            c.changed(3);
            CHECK(c.save(3, true));
            std::ofstream(rootPath() + "/config/state.ini") << "truncated";
            CHECK(restored.load());
            CHECK(restored.lastBook == b.path);
        }
        {
            Gestures g;
            CHECK(g.update({{1, 300, 300}}, 0, false).type == GestureType::None);
            CHECK(g.update({}, .1, false).type == GestureType::Tap);
            g.update({{1, 300, 300}}, 1, false);
            g.update({{1, 100, 300}}, 1.2, false);
            CHECK(g.update({}, 1.3, false).type == GestureType::Swipe);
            g.update({{1, 100, 100}}, 2, false);
            CHECK(g.update({{1, 100, 100}, {2, 200, 100}}, 2.1, false).type == GestureType::Pinch);
            CHECK(g.update({{1, 100, 100}, {2, 300, 100}}, 2.2, false).scale == 2);
            CHECK(g.update({{1, 100, 100}}, 2.3, false).type == GestureType::None);
            CHECK(g.update({}, 2.4, false).type == GestureType::Release);
            g.update({{1, 100, 100}}, 3, false);
            CHECK(g.update({{1, 100, 100}}, 3.7, false).type == GestureType::LongPress);
            CHECK(g.update({}, 3.8, false).type == GestureType::Release);
        }
        {
            Worker worker(fonts);
            Request r;
            r.kind = Request::Open;
            r.path = fixture + "book.pdf";
            r.settings = settings;
            worker.submit(r);
            auto wait = [&](uint64_t after) {
                ReaderView v;
                for (int i = 0; i < 2500; ++i) {
                    bool busy;
                    v = worker.snapshot(busy);
                    if (!busy && v.revision > after)
                        return v;
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
                throw std::runtime_error("Worker timeout");
            };
            auto v = wait(0);
            CHECK(v.error.empty());
            CHECK(v.pages == 2);
            CHECK(v.next != nullptr);
            r.kind = Request::Zoom;
            r.zoom = 2;
            worker.submit(r);
            v = wait(v.revision);
            CHECK(v.position.zoom == 2);
            r.zoom = 1;
            worker.submit(r);
            v = wait(v.revision);
            CHECK(v.position.zoom == 1);
            r.kind = Request::Page;
            r.delta = 1;
            worker.submit(r);
            v = wait(v.revision);
            CHECK(v.position.page == 1);
            CHECK(v.position.zoom == 1);
            r.kind = Request::Close;
            worker.submit(r);
            v = wait(v.revision);
            CHECK(v.path.empty());
            worker.stop();
        }
        {
            std::string dir = rootPath() + "/books";
            mkdir((dir + "/nested").c_str(), 0777);
            std::ofstream(dir + "/nested/Тест.txt") << "hello";
            std::ofstream(dir + "/ignore.azw") << "ignored";
            symlink((dir + "/nested").c_str(), (dir + "/link").c_str());
            auto list = scanLibrary(dir, cancel);
            CHECK(list.size() == 1);
            CHECK(list[0].format == "txt");
        }
        std::cout << "PASS: " << assertions << " checks\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
