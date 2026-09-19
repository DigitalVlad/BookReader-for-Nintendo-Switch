#pragma once
#include "readers/IBookReader.hpp"
#include "utils/MemoryManager.hpp"
#include <list>
#include <mupdf/fitz.h>
namespace br {
class MuReader : public IBookReader {
  protected:
    MemoryManager memory_;
    fz_context* ctx_ = nullptr;
    fz_document* doc_ = nullptr;
    std::string path_, documentPath_, title_, author_, error_;
    Settings settings_;
    Position pos_;
    int count_ = 0;
    bool fixed_ = false;
    std::atomic<bool>& cancel_;
    std::vector<Chapter> chapters_;
    struct Cached {
        int page;
        float zoom;
        std::shared_ptr<Bitmap> bitmap;
    };
    std::list<Cached> cache_;
    void loadDocument();
    void outline();
    void clearDocument();
    virtual void extraContents() {}
    virtual std::string prepare(const std::string& path) { return path; }

  public:
    explicit MuReader(std::atomic<bool>& cancel);
    ~MuReader() override;
    bool open(const std::string& path) override;
    void close() override;
    std::string getTitle() const override { return title_; }
    std::string getAuthor() const override { return author_; }
    int getPageCount() const override { return count_; }
    int getCurrentPage() const override { return pos_.page; }
    void goToPage(int p) override;
    void configure(const Settings& s) override;
    bool fixedLayout() const override { return fixed_; }
    Position position() const override;
    void restore(const Position& p) override;
    std::shared_ptr<Bitmap> render(float zoom) override;
    std::vector<Chapter> contents() const override { return chapters_; }
    std::string error() const override { return error_; }
};
class PdfReader final : public MuReader {
  public:
    using MuReader::MuReader;
};
class EpubReader final : public MuReader {
    struct Link {
        std::string title, uri;
        int level;
    };
    std::vector<Link> nav_;
    std::string coverPath_;
    std::string prepare(const std::string& path) override;
    void extraContents() override;

  public:
    using MuReader::MuReader;
    std::shared_ptr<Bitmap> cover() override;
};
class HtmlReader final : public MuReader {
  public:
    using MuReader::MuReader;
};
class CbzReader final : public MuReader {
  public:
    using MuReader::MuReader;
};
class Fb2Reader final : public MuReader {
    std::string prepare(const std::string& path) override;

  public:
    using MuReader::MuReader;
};
} // namespace br
