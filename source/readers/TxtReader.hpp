#pragma once
#include "readers/IBookReader.hpp"
#include "render/Fonts.hpp"
#include <cstdio>
namespace br {
class TxtReader final : public IBookReader {
    FILE* file_ = nullptr;
    std::string path_, error_;
    Settings settings_;
    Position pos_;
    std::vector<uint64_t> pages_;
    std::vector<FontSource> sources_;
    std::unique_ptr<Fonts> fonts_;
    std::atomic<bool>& cancel_;
    uint64_t size_ = 0, bom_ = 0;
    int encoding_ = 0;
    uint32_t readCodepoint();
    bool line(std::string& result);
    void index();

  public:
    TxtReader(std::vector<FontSource> sources, std::atomic<bool>& cancel)
        : sources_(std::move(sources)), cancel_(cancel) {}
    ~TxtReader() override { close(); }
    bool open(const std::string& path) override;
    void close() override;
    std::string getTitle() const override;
    std::string getAuthor() const override { return {}; }
    int getPageCount() const override { return int(pages_.size()); }
    int getCurrentPage() const override { return pos_.page; }
    void goToPage(int p) override;
    void configure(const Settings& s) override;
    bool fixedLayout() const override { return false; }
    Position position() const override;
    void restore(const Position& p) override;
    std::shared_ptr<Bitmap> render(float zoom) override;
    std::vector<Chapter> contents() const override { return {}; }
    std::string error() const override { return error_; }
};
} // namespace br
