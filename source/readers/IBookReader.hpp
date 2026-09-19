#pragma once
#include "utils/Types.hpp"
#include <atomic>
#include <memory>
namespace br {
class IBookReader {
  public:
    virtual bool open(const std::string& path) = 0;
    virtual void close() = 0;
    virtual std::string getTitle() const = 0;
    virtual std::string getAuthor() const = 0;
    virtual int getPageCount() const = 0;
    virtual int getCurrentPage() const = 0;
    virtual void goToPage(int page) = 0;
    virtual void nextPage() { goToPage(getCurrentPage() + 1); }
    virtual void previousPage() { goToPage(getCurrentPage() - 1); }
    virtual float getProgress() const {
        return getPageCount() > 1 ? float(getCurrentPage()) / float(getPageCount() - 1) : 1.f;
    }
    virtual void configure(const Settings& settings) = 0;
    virtual bool fixedLayout() const = 0;
    virtual Position position() const = 0;
    virtual void restore(const Position& p) = 0;
    virtual std::shared_ptr<Bitmap> render(float zoom) = 0;
    virtual std::vector<Chapter> contents() const = 0;
    virtual std::shared_ptr<Bitmap> cover() {
        auto p = position();
        goToPage(0);
        try {
            auto b = render(1);
            restore(p);
            return b;
        } catch (...) {
            restore(p);
            throw;
        }
    }
    virtual std::string error() const = 0;
    virtual ~IBookReader() = default;
};
} // namespace br
