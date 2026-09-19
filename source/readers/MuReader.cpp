#include "readers/MuReader.hpp"
#include "utils/Files.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>
namespace br {
MuReader::MuReader(std::atomic<bool>& cancel) : cancel_(cancel) {
    fz_alloc_context alloc{&memory_, MemoryManager::allocate, MemoryManager::resize,
                           MemoryManager::release};
    ctx_ = fz_new_context(&alloc, nullptr, 24 * 1024 * 1024);
    if (!ctx_)
        throw std::runtime_error("Not enough memory: MuPDF context");
    fz_try(ctx_) {
        fz_register_document_handlers(ctx_);
    }
    fz_catch(ctx_) {
        error_ = fz_caught_message(ctx_);
    }
    if (!error_.empty()) {
        fz_drop_context(ctx_);
        ctx_ = nullptr;
        throw std::runtime_error(error_);
    }
}
MuReader::~MuReader() {
    close();
    fz_drop_context(ctx_);
}
void MuReader::clearDocument() {
    cache_.clear();
    chapters_.clear();
    fz_drop_document(ctx_, doc_);
    doc_ = nullptr;
    count_ = 0;
    fz_empty_store(ctx_);
}
void MuReader::close() {
    clearDocument();
    path_.clear();
    documentPath_.clear();
    pos_ = {};
}
static void flattenOutline(fz_context* ctx, fz_document* doc, fz_outline* n,
                           std::vector<Chapter>& out, int level) {
    if (level > 32)
        return;
    for (; n && out.size() < 4096; n = n->next) {
        int page = n->page;
        if (page < 0 && n->uri) {
            fz_try(ctx) {
                page = fz_page_number_from_location(
                    ctx, doc, fz_resolve_link(ctx, doc, n->uri, nullptr, nullptr));
            }
            fz_catch(ctx) {
                page = -1;
            }
        }
        if (n->title && page >= 0)
            out.push_back({n->title, page, level});
        flattenOutline(ctx, doc, n->down, out, level + 1);
    }
}
void MuReader::outline() {
    fz_outline* tree = nullptr;
    fz_var(tree);
    fz_try(ctx_) {
        tree = fz_load_outline(ctx_, doc_);
    }
    fz_catch(ctx_) {
        tree = nullptr;
    }
    try {
        flattenOutline(ctx_, doc_, tree, chapters_, 0);
    } catch (...) {
        fz_drop_outline(ctx_, tree);
        throw;
    }
    fz_drop_outline(ctx_, tree);
}
void MuReader::loadDocument() {
    auto pal = palette(settings_.theme);
    char css[1400];
    const char* family = settings_.font % 3 == 1   ? "sans-serif"
                         : settings_.font % 3 == 2 ? "monospace"
                                                   : "serif";
    snprintf(css, sizeof(css),
             "@page { margin: %dpx; } html,body { margin: 0; color: rgb(%u,%u,%u); "
             "background-color: rgb(%u,%u,%u); font-family: %s; line-height: %.2f; text-align: %s; "
             "} p,div,li { line-height: %.2f; }",
             settings_.margin, pal.text.r, pal.text.g, pal.text.b, pal.background.r,
             pal.background.g, pal.background.b, family, double(settings_.lineSpacing),
             settings_.alignment == 1   ? "center"
             : settings_.alignment == 2 ? "right"
                                        : "left",
             double(settings_.lineSpacing));
    bool ok = true;
    char msg[256]{};
    char title[512]{}, author[512]{};
    fz_try(ctx_) {
        fz_set_user_css(ctx_, css);
        fz_set_use_document_css(ctx_, 1);
        doc_ = fz_open_document(ctx_, documentPath_.c_str());
        if (fz_needs_password(ctx_, doc_) && !fz_authenticate_password(ctx_, doc_, ""))
            fz_throw(ctx_, FZ_ERROR_GENERIC, "Password-protected books are not supported");
        fixed_ = !fz_is_document_reflowable(ctx_, doc_);
        if (!fixed_)
            fz_layout_document(ctx_, doc_, float(settings_.width()), float(settings_.height()),
                               float(settings_.fontSize));
        count_ = fz_count_pages(ctx_, doc_);
        fz_lookup_metadata(ctx_, doc_, FZ_META_INFO_TITLE, title, sizeof(title));
        fz_lookup_metadata(ctx_, doc_, FZ_META_INFO_AUTHOR, author, sizeof(author));
    }
    fz_catch(ctx_) {
        ok = false;
        snprintf(msg, sizeof(msg), "%s", fz_caught_message(ctx_));
    }
    if (!ok || count_ <= 0) {
        clearDocument();
        throw std::runtime_error(ok ? "Cannot open book: no pages" : msg);
    }
    if (title[0])
        title_ = title;
    if (author[0])
        author_ = author;
    outline();
    extraContents();
}
bool MuReader::open(const std::string& path) {
    close();
    error_.clear();
    path_ = path;
    title_ = basename(path);
    author_.clear();
    try {
        struct stat st{};
        if (stat(path.c_str(), &st))
            throw std::runtime_error("Cannot open book");
        auto ext = extension(path);
        if ((ext == "epub" && st.st_size > 128 * 1024 * 1024) ||
            ((ext == "fb2" || ext == "html" || ext == "htm") && st.st_size > 24 * 1024 * 1024))
            throw std::runtime_error("Book exceeds safe reflow input size");
        documentPath_ = prepare(path);
        loadDocument();
        return true;
    } catch (const std::exception& e) {
        error_ = e.what();
        clearDocument();
        return false;
    }
}
void MuReader::configure(const Settings& s) {
    auto old = position();
    settings_ = s;
    settings_.clamp();
    if (!doc_)
        return;
    if (fixed_) {
        cache_.clear();
        return;
    }
    // CSS is applied at parse time in MuPDF 1.17. Reopen to apply theme/style.
    // Page fraction is used across the new layout; on unchanged settings restore is exact.
    float fraction = getProgress();
    clearDocument();
    loadDocument();
    old.page = int(std::round(fraction * std::max(0, count_ - 1)));
    restore(old);
}
void MuReader::goToPage(int p) {
    pos_.page = std::clamp(p, 0, std::max(0, count_ - 1));
}
Position MuReader::position() const {
    auto p = pos_;
    p.progress = getProgress();
    p.chapter = 0;
    for (size_t i = 0; i < chapters_.size(); ++i)
        if (chapters_[i].page <= p.page)
            p.chapter = int(i);
    return p;
}
void MuReader::restore(const Position& p) {
    pos_ = p;
    pos_.zoom = std::clamp(p.zoom, 1.f, 4.f);
    goToPage(p.page);
}
std::shared_ptr<Bitmap> MuReader::render(float zoom) {
    if (!doc_)
        throw std::runtime_error("Cannot render a closed book");
    if (cancel_)
        throw std::runtime_error("Cancelled");
    zoom = fixed_ ? std::clamp(zoom, 1.f, 4.f) : 1;
    for (auto i = cache_.begin(); i != cache_.end(); ++i)
        if (i->page == pos_.page && std::abs(i->zoom - zoom) < .01f) {
            auto bitmap = i->bitmap;
            cache_.splice(cache_.begin(), cache_, i);
            pos_.zoom = zoom;
            return bitmap;
        }
    // LRU holds at most three pages. Keep neighbor fit rasters when current zoom changes.
    for (auto i = cache_.begin(); i != cache_.end();)
        if (i->page == pos_.page && std::abs(i->zoom - zoom) > .01f)
            i = cache_.erase(i);
        else
            ++i;
    while (cache_.size() >= 3)
        cache_.pop_back();
    fz_page* page = nullptr;
    fz_pixmap* pix = nullptr;
    fz_device* dev = nullptr;
    fz_var(page);
    fz_var(pix);
    fz_var(dev);
    bool ok = true;
    char msg[256]{};
    fz_cookie cookie{};
    fz_try(ctx_) {
        page = fz_load_page(ctx_, doc_, pos_.page);
        fz_rect bounds = fz_bound_page(ctx_, page);
        float w = bounds.x1 - bounds.x0, h = bounds.y1 - bounds.y0;
        if (!std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0)
            fz_throw(ctx_, FZ_ERROR_GENERIC, "PDF rendering failed: invalid page bounds");
        float scale = std::min(float(settings_.width()) / w, float(settings_.height()) / h) * zoom;
        scale = std::min(scale, 2048.f / std::max(w, h));
        scale = std::min(scale, float(std::sqrt((4.0 * 1024 * 1024) / (double(w) * h))));
        fz_matrix matrix = fz_scale(scale, scale);
        fz_irect box = fz_round_rect(fz_transform_rect(bounds, matrix));
        if (box.x1 - box.x0 > 2048 || box.y1 - box.y0 > 2048)
            fz_throw(ctx_, FZ_ERROR_GENERIC, "Page raster limit");
        pix = fz_new_pixmap_with_bbox(ctx_, fz_device_rgb(ctx_), box, nullptr, 0);
        fz_clear_pixmap_with_value(ctx_, pix, 255);
        if (!fixed_) {
            auto color = palette(settings_.theme).background;
            int pw = fz_pixmap_width(ctx_, pix), ph = fz_pixmap_height(ctx_, pix),
                stride = fz_pixmap_stride(ctx_, pix);
            unsigned char* sample = fz_pixmap_samples(ctx_, pix);
            for (int py = 0; py < ph; ++py)
                for (int px = 0; px < pw; ++px) {
                    auto* p = sample + py * stride + px * 3;
                    p[0] = color.r;
                    p[1] = color.g;
                    p[2] = color.b;
                }
        }
        dev = fz_new_draw_device(ctx_, fz_identity, pix);
        fz_run_page(ctx_, page, dev, matrix, &cookie);
        fz_close_device(ctx_, dev);
    }
    fz_always(ctx_) {
        fz_drop_device(ctx_, dev);
        fz_drop_page(ctx_, page);
    }
    fz_catch(ctx_) {
        ok = false;
        snprintf(msg, sizeof(msg), "%s", fz_caught_message(ctx_));
    }
    if (!ok) {
        fz_drop_pixmap(ctx_, pix);
        throw std::runtime_error(std::string("PDF/EPUB rendering failed: ") + msg);
    }
    std::shared_ptr<Bitmap> out;
    try {
        out = std::make_shared<Bitmap>();
        int w = fz_pixmap_width(ctx_, pix), h = fz_pixmap_height(ctx_, pix),
            n = fz_pixmap_components(ctx_, pix), stride = fz_pixmap_stride(ctx_, pix);
        out->width = w;
        out->height = h;
        out->rgba.resize(size_t(w) * h * 4);
        const auto* pixels = fz_pixmap_samples(ctx_, pix);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const auto* in = pixels + y * stride + x * n;
                auto* p = &out->rgba[(size_t(y) * w + x) * 4];
                p[0] = in[0];
                p[1] = in[n > 1 ? 1 : 0];
                p[2] = in[n > 2 ? 2 : 0];
                p[3] = 255;
            }
    } catch (...) {
        fz_drop_pixmap(ctx_, pix);
        throw;
    }
    fz_drop_pixmap(ctx_, pix);
    cache_.push_front({pos_.page, zoom, out});
    pos_.zoom = zoom;
    return out;
}
} // namespace br
