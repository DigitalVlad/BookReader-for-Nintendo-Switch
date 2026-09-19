#pragma once
#include "readers/BookFactory.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
namespace br {
struct ReaderView {
    std::string path, title, author, error;
    int pages = 0;
    Position position;
    Settings settings;
    bool fixed = false;
    std::vector<Chapter> chapters;
    std::shared_ptr<Bitmap> bitmap, previous, next;
    uint64_t revision = 0;
};
struct Request {
    enum Kind { Open, Page, Jump, Configure, Zoom, Close } kind = Page;
    std::string path;
    Settings settings;
    Position position;
    int delta = 0;
    float zoom = 1;
};
class Worker {
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Request> queue_;
    std::unique_ptr<IBookReader> reader_;
    ReaderView view_;
    uint64_t revision_ = 0;
    bool busy_ = false, stopping_ = false;
    std::vector<FontSource> fonts_;
    std::atomic<bool> cancel_{false};
    void run();
    void cover();

  public:
    explicit Worker(std::vector<FontSource> fonts);
    ~Worker();
    void submit(Request request);
    ReaderView snapshot(bool& busy, uint64_t afterRevision = 0);
    void stop();
};
} // namespace br
