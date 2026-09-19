#include "app/Worker.hpp"
#include "utils/Files.hpp"
#include <SDL.h>
#include <stdexcept>
#include <sys/stat.h>
namespace br {
Worker::Worker(std::vector<FontSource> fonts) : fonts_(std::move(fonts)) {
    thread_ = std::thread(&Worker::run, this);
}
Worker::~Worker() {
    stop();
}
void Worker::stop() {
    {
        std::lock_guard<std::mutex> l(mutex_);
        stopping_ = true;
        cancel_ = true;
        queue_.clear();
    }
    wake_.notify_one();
    if (thread_.joinable())
        thread_.join();
}
void Worker::submit(Request r) {
    std::lock_guard<std::mutex> l(mutex_);
    if (stopping_)
        return;
    if (r.kind == Request::Open || r.kind == Request::Close) {
        cancel_ = true;
        queue_.clear();
    } else if (!queue_.empty() && queue_.back().kind == r.kind &&
               (r.kind == Request::Zoom || r.kind == Request::Configure))
        queue_.pop_back();
    if (queue_.size() < 16)
        queue_.push_back(std::move(r));
    wake_.notify_one();
}
ReaderView Worker::snapshot(bool& busy, uint64_t afterRevision) {
    std::lock_guard<std::mutex> l(mutex_);
    busy = busy_ || !queue_.empty();
    if (view_.revision == afterRevision) {
        ReaderView unchanged;
        unchanged.revision = afterRevision;
        return unchanged;
    }
    return view_;
}
void Worker::cover() {
    if (!reader_)
        return;
    auto dest = rootPath() + "/cache/covers/" + fingerprint(view_.path) + ".bmp";
    struct stat st{};
    if (!stat(dest.c_str(), &st))
        return;
    try {
        auto b = reader_->cover();
        Bitmap small;
        small.width = 120;
        small.height = 168;
        small.rgba.resize(120 * 168 * 4);
        for (int y = 0; y < 168; ++y)
            for (int x = 0; x < 120; ++x) {
                int sx = x * b->width / 120, sy = y * b->height / 168;
                std::copy_n(&b->rgba[(size_t(sy) * b->width + sx) * 4], 4,
                            &small.rgba[(y * 120 + x) * 4]);
            }
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(small.rgba.data(), 120, 168, 32,
                                                                  120 * 4, SDL_PIXELFORMAT_RGBA32);
        if (surface) {
            auto tmp = dest + ".tmp";
            if (SDL_SaveBMP(surface, tmp.c_str()) == 0)
                rename(tmp.c_str(), dest.c_str());
            SDL_FreeSurface(surface);
        }
        trimCache(rootPath() + "/cache/covers", 32 * 1024 * 1024);
    } catch (...) { /* Thumbnail failure must not prevent reading. */
    }
}
void Worker::run() {
    Settings current;
    ReaderView next;
    while (true) {
        Request req;
        {
            std::unique_lock<std::mutex> l(mutex_);
            wake_.wait(l, [&] { return stopping_ || !queue_.empty(); });
            if (stopping_)
                break;
            req = queue_.front();
            queue_.pop_front();
            busy_ = true;
            cancel_ = false;
            next = view_;
        }
        try {
            next.error.clear();
            if (req.kind == Request::Close) {
                reader_.reset();
                next = {};
            } else {
                if (req.kind == Request::Open) {
                    reader_.reset();
                    next = {};
                    next.path = req.path;
                    current = req.settings;
                    reader_ = BookFactory::open(req.path, current, fonts_, cancel_);
                    reader_->restore(req.position);
                }
                if (!reader_)
                    throw std::runtime_error("No book is open");
                if (req.kind == Request::Page)
                    reader_->goToPage(reader_->getCurrentPage() + req.delta);
                if (req.kind == Request::Jump)
                    reader_->goToPage(req.delta);
                if (req.kind == Request::Configure) {
                    current = req.settings;
                    reader_->configure(current);
                }
                auto pos = reader_->position();
                if (req.kind == Request::Zoom)
                    pos.zoom = req.zoom;
                next.bitmap = reader_->render(pos.zoom);
                next.position = reader_->position();
                next.position.zoom = pos.zoom;
                next.title = reader_->getTitle();
                next.author = reader_->getAuthor();
                next.pages = reader_->getPageCount();
                next.fixed = reader_->fixedLayout();
                next.chapters = reader_->contents();
                next.settings = current;
                next.previous.reset();
                next.next.reset();
                if (current.scrollMode || next.fixed) {
                    auto saved = reader_->position();
                    try {
                        if (saved.page > 0) {
                            reader_->goToPage(saved.page - 1);
                            next.previous = reader_->render(1);
                        }
                        if (saved.page + 1 < reader_->getPageCount()) {
                            reader_->goToPage(saved.page + 1);
                            next.next = reader_->render(1);
                        }
                    } catch (...) {
                        next.previous.reset();
                        next.next.reset();
                    }
                    reader_->restore(saved);
                }
            }
        } catch (const std::bad_alloc&) {
            next.error = "Not enough memory";
        } catch (const std::exception& e) {
            next.error = e.what();
        } catch (...) {
            next.error = "Cannot open book";
        }
        {
            std::lock_guard<std::mutex> l(mutex_);
            next.revision = ++revision_;
            view_ = std::move(next);
            busy_ = false;
        }
        // Generate cover after publishing the page. Only the worker touches the reader.
        if (req.kind == Request::Open && reader_ && !cancel_)
            cover();
    }
    reader_.reset();
}
} // namespace br
