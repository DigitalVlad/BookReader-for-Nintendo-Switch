#include "readers/TxtReader.hpp"
#include "utils/Files.hpp"
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>
namespace br {
std::string TxtReader::getTitle() const {
    return basename(path_);
}
void TxtReader::close() {
    if (file_)
        fclose(file_);
    file_ = nullptr;
    pages_.clear();
    fonts_.reset();
    pos_ = {};
}
uint32_t TxtReader::readCodepoint() {
    int c = fgetc(file_);
    if (c == EOF)
        return 0xFFFFFFFF;
    if (encoding_) {
        int d = fgetc(file_);
        if (d == EOF)
            return 0xFFFD;
        uint32_t v = encoding_ == 1 ? c | (d << 8) : (c << 8) | d;
        if (v >= 0xD800 && v <= 0xDBFF) {
            auto before = ftello(file_);
            int a = fgetc(file_), b = fgetc(file_);
            if (a == EOF || b == EOF)
                return 0xFFFD;
            uint32_t lo = encoding_ == 1 ? a | (b << 8) : (a << 8) | b;
            if (lo >= 0xDC00 && lo <= 0xDFFF)
                return 0x10000 + ((v - 0xD800) << 10) + (lo - 0xDC00);
            fseeko(file_, before, SEEK_SET);
            return 0xFFFD;
        }
        return v >= 0xDC00 && v <= 0xDFFF ? 0xFFFD : v;
    }
    if (c < 128)
        return c;
    int n = c >= 0xC2 && c <= 0xDF   ? 1
            : c >= 0xE0 && c <= 0xEF ? 2
            : c >= 0xF0 && c <= 0xF4 ? 3
                                     : 0;
    if (!n)
        return 0xFFFD;
    std::string bytes(1, char(c));
    for (int i = 0; i < n; ++i) {
        int d = fgetc(file_);
        if (d == EOF)
            return 0xFFFD;
        if ((d & 0xC0) != 0x80) {
            ungetc(d, file_);
            return 0xFFFD;
        }
        bytes += char(d);
    }
    size_t at = 0;
    return utf8Next(bytes, at);
}
bool TxtReader::line(std::string& out) {
    out.clear();
    int width = 0;
    bool any = false;
    uint64_t wordOffset = 0;
    size_t wordByte = 0;
    int wordWidth = 0;
    const int limit = settings_.width() - 2 * settings_.margin;
    while (true) {
        uint64_t before = uint64_t(ftello(file_));
        auto c = readCodepoint();
        if (c == 0xFFFFFFFF)
            return any;
        any = true;
        if (c == '\n')
            return true;
        if (c == '\r')
            continue;
        if (c == '\t')
            c = ' ';
        if (c == 0)
            c = 0xFFFD;
        int advance = fonts_->advance(c);
        if (width + advance > limit && !out.empty()) {
            if (wordByte > 0 && wordOffset > 0 && width - wordWidth < limit / 2) {
                fseeko(file_, off_t(wordOffset), SEEK_SET);
                out.resize(wordByte);
            } else
                fseeko(file_, off_t(before), SEEK_SET);
            return true;
        }
        out += utf8(c);
        width += advance;
        if (c == ' ') {
            wordOffset = uint64_t(ftello(file_));
            wordByte = out.size();
            wordWidth = width;
        }
    }
}
void TxtReader::index() {
    pages_.clear();
    auto sig = fingerprint(path_) + "-" + std::to_string(settings_.width()) + "-" +
               std::to_string(settings_.height()) + "-" + std::to_string(settings_.fontSize) + "-" +
               std::to_string(settings_.margin) + "-" + std::to_string(settings_.lineSpacing) +
               "-" + std::to_string(settings_.font);
    for (auto& s : sources_)
        sig += s.path + std::to_string(s.size) + fingerprint(s.path);
    auto cache = rootPath() + "/cache/text/" + keyFor(sig) + ".idx";
    FILE* f = fopen(cache.c_str(), "rb");
    if (f) {
        uint64_t n = 0;
        char magic[8]{};
        bool valid = fread(magic, 1, 8, f) == 8 && !memcmp(magic, "BRTXT002", 8) &&
                     fread(&n, 8, 1, f) == 1 && n > 0 && n <= 1000000;
        if (valid) {
            pages_.resize(size_t(n));
            valid = fread(pages_.data(), 8, size_t(n), f) == n && pages_[0] == bom_;
            for (size_t i = 0; valid && i < pages_.size(); ++i)
                valid = pages_[i] <= size_ && (!i || pages_[i] > pages_[i - 1]);
        }
        fclose(f);
        if (valid)
            return;
        pages_.clear();
    }
    fseeko(file_, off_t(bom_), SEEK_SET);
    std::string text;
    int perPage = std::max(1, int((settings_.height() - 2 * settings_.margin) /
                                  (settings_.fontSize * settings_.lineSpacing)));
    bool eof = false;
    while (!eof) {
        if (cancel_)
            throw std::runtime_error("Cancelled");
        uint64_t p = uint64_t(ftello(file_));
        if (p >= size_ && !pages_.empty())
            break;
        pages_.push_back(p);
        if (pages_.size() > 1000000)
            throw std::runtime_error("TXT exceeds page-index limit");
        for (int i = 0; i < perPage; ++i) {
            if (!line(text)) {
                eof = true;
                break;
            }
        }
    }
    std::string data("BRTXT002", 8);
    uint64_t count = pages_.size();
    data.append(reinterpret_cast<char*>(&count), 8);
    data.append(reinterpret_cast<const char*>(pages_.data()), pages_.size() * 8);
    atomicWrite(cache, data);
    trimCache(rootPath() + "/cache/text", 32 * 1024 * 1024);
}
bool TxtReader::open(const std::string& p) {
    close();
    path_ = p;
    error_.clear();
    try {
        file_ = fopen(p.c_str(), "rb");
        if (!file_)
            throw std::runtime_error("Cannot open book");
        fseeko(file_, 0, SEEK_END);
        size_ = uint64_t(ftello(file_));
        if (size_ > 512ULL * 1024 * 1024)
            throw std::runtime_error("TXT limit: 512 MiB");
        rewind(file_);
        unsigned char b[3]{};
        size_t n = fread(b, 1, 3, file_);
        bom_ = 0;
        encoding_ = 0;
        if (n >= 2 && b[0] == 255 && b[1] == 254) {
            encoding_ = 1;
            bom_ = 2;
        } else if (n >= 2 && b[0] == 254 && b[1] == 255) {
            encoding_ = 2;
            bom_ = 2;
        } else if (n == 3 && b[0] == 239 && b[1] == 187 && b[2] == 191)
            bom_ = 3;
        configure(settings_);
        return true;
    } catch (const std::exception& e) {
        error_ = e.what();
        close();
        return false;
    }
}
void TxtReader::configure(const Settings& s) {
    auto saved = position();
    settings_ = s;
    settings_.clamp();
    auto sources = sources_;
    if (settings_.font > 0 && size_t(settings_.font - 1) < sources.size())
        std::rotate(sources.begin(), sources.begin() + settings_.font - 1,
                    sources.begin() + settings_.font);
    fonts_ = std::make_unique<Fonts>(sources);
    if (!fonts_->valid())
        throw std::runtime_error("No fonts. Add a TTF to fonts/");
    fonts_->size(settings_.fontSize);
    if (file_) {
        index();
        restore(saved);
    }
}
void TxtReader::goToPage(int p) {
    pos_.page = std::clamp(p, 0, std::max(0, getPageCount() - 1));
}
Position TxtReader::position() const {
    auto p = pos_;
    if (!pages_.empty())
        p.offset = pages_[size_t(pos_.page)];
    p.progress = getProgress();
    return p;
}
void TxtReader::restore(const Position& p) {
    pos_ = p;
    if (!pages_.empty()) {
        auto it = std::upper_bound(pages_.begin(), pages_.end(), p.offset);
        goToPage(int(it - pages_.begin()) - 1);
    }
}
std::shared_ptr<Bitmap> TxtReader::render(float) {
    auto out = std::make_shared<Bitmap>();
    auto pal = palette(settings_.theme);
    fill(*out, settings_.width(), settings_.height(), pal.background);
    fseeko(file_, off_t(pages_.at(pos_.page)), SEEK_SET);
    int lines = std::max(1, int((settings_.height() - 2 * settings_.margin) /
                                (settings_.fontSize * settings_.lineSpacing)));
    std::string text;
    for (int i = 0; i < lines && line(text); ++i) {
        int w = fonts_->width(text);
        int x = settings_.margin;
        if (settings_.alignment == 1)
            x = (settings_.width() - w) / 2;
        if (settings_.alignment == 2)
            x = settings_.width() - settings_.margin - w;
        fonts_->draw(*out, x,
                     settings_.margin + settings_.fontSize +
                         int(i * settings_.fontSize * settings_.lineSpacing),
                     text, pal.text);
    }
    return out;
}
} // namespace br
