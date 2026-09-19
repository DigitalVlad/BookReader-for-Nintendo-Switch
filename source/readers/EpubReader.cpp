#include "readers/MuReader.hpp"
#include "utils/Files.hpp"
#include <functional>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
namespace br {
// Resource paths stay inside the ZIP. Never extract archive-supplied filesystem paths.
static std::string zipPath(const std::string& base, const std::string& rel) {
    if (rel.find(":") != std::string::npos)
        throw std::runtime_error("External EPUB resource is unsupported");
    std::string joined = base.empty() ? rel
                                      : base.substr(0, base.find_last_of('/') == std::string::npos
                                                           ? 0
                                                           : base.find_last_of('/') + 1) +
                                            rel;
    std::vector<std::string> parts;
    std::istringstream in(joined);
    std::string p;
    while (std::getline(in, p, '/')) {
        if (p.empty() || p == ".")
            continue;
        if (p == "..") {
            if (parts.empty())
                throw std::runtime_error("Invalid EPUB resource path");
            parts.pop_back();
        } else
            parts.push_back(p);
    }
    std::string out;
    for (auto& s : parts) {
        if (!out.empty())
            out += '/';
        out += s;
    }
    return out;
}
static std::string xmlText(fz_xml* n, int depth = 0) {
    if (depth > 32)
        return {};
    std::string out;
    for (; n; n = fz_xml_next(n)) {
        if (auto* t = fz_xml_text(n))
            out += t;
        else
            out += xmlText(fz_xml_down(n), depth + 1);
        if (out.size() > 4096) {
            out.resize(4096);
            break;
        }
    }
    return out;
}
struct Xml {
    fz_context* c;
    fz_xml_doc* p = nullptr;
    ~Xml() { fz_drop_xml(c, p); }
};
struct Archive {
    fz_context* c;
    fz_archive* p = nullptr;
    ~Archive() { fz_drop_archive(c, p); }
};
static fz_buffer* readEntry(fz_context* c, fz_archive* a, const std::string& name, size_t limit) {
    fz_stream* s = nullptr;
    fz_buffer* b = nullptr;
    fz_var(s);
    fz_var(b);
    bool ok = true;
    char message[256]{};
    fz_try(c) {
        s = fz_open_archive_entry(c, a, name.c_str());
        b = fz_new_buffer(c, 4096);
        unsigned char bytes[4096];
        size_t n, total = 0;
        while ((n = fz_read(c, s, bytes, sizeof(bytes))) > 0) {
            total += n;
            if (total > limit)
                fz_throw(c, FZ_ERROR_GENERIC, "EPUB resource size limit");
            fz_append_data(c, b, bytes, n);
        }
    }
    fz_always(c) {
        fz_drop_stream(c, s);
    }
    fz_catch(c) {
        ok = false;
        snprintf(message, sizeof(message), "%s", fz_caught_message(c));
    }
    if (!ok) {
        fz_drop_buffer(c, b);
        throw std::runtime_error(message);
    }
    return b;
}
static void parseEntry(Xml& xml, fz_archive* a, const std::string& name) {
    fz_buffer* b = readEntry(xml.c, a, name, 2 * 1024 * 1024);
    bool ok = true;
    fz_try(xml.c) {
        xml.p = fz_parse_xml(xml.c, b, 0, 0);
    }
    fz_always(xml.c) {
        fz_drop_buffer(xml.c, b);
    }
    fz_catch(xml.c) {
        ok = false;
    }
    if (!ok)
        throw std::runtime_error("Invalid EPUB XML: " + name);
}
static const char* att(fz_xml* n, const char* k) {
    auto* v = fz_xml_att(n, k);
    return v ? v : "";
}
std::string EpubReader::prepare(const std::string& path) {
    nav_.clear();
    coverPath_.clear();
    Archive archive{ctx_};
    bool ok = true;
    fz_try(ctx_) {
        archive.p = fz_open_archive(ctx_, path.c_str());
    }
    fz_catch(ctx_) {
        ok = false;
    }
    if (!ok)
        throw std::runtime_error("Invalid EPUB file");
    Xml container{ctx_};
    parseEntry(container, archive.p, "META-INF/container.xml");
    auto* files = fz_xml_find_down(fz_xml_root(container.p), "rootfiles");
    auto* rootfile = fz_xml_find_down(files, "rootfile");
    std::string opf = att(rootfile, "full-path");
    if (opf.empty())
        throw std::runtime_error("Invalid EPUB: missing OPF");
    opf = zipPath("", opf);
    Xml package{ctx_};
    parseEntry(package, archive.p, opf);
    auto* root = fz_xml_root(package.p);
    auto* metadata = fz_xml_find_down(root, "metadata");
    std::string coverId, cover, nav;
    for (auto* n = fz_xml_down(metadata); n; n = fz_xml_next(n)) {
        std::string tag = fz_xml_tag(n) ? fz_xml_tag(n) : "";
        if (tag == "dc:title" || tag == "title")
            title_ = xmlText(fz_xml_down(n));
        if (tag == "dc:creator" || tag == "creator")
            author_ = xmlText(fz_xml_down(n));
        if (std::string(att(n, "name")) == "cover")
            coverId = att(n, "content");
    }
    auto* manifest = fz_xml_find_down(root, "manifest");
    for (auto* n = fz_xml_down(manifest); n; n = fz_xml_next(n)) {
        std::string properties = att(n, "properties"), id = att(n, "id"), href = att(n, "href");
        if (href.empty())
            continue;
        if (properties.find("nav") != std::string::npos)
            nav = zipPath(opf, href);
        if (properties.find("cover-image") != std::string::npos ||
            (!coverId.empty() && id == coverId))
            cover = zipPath(opf, href);
    }
    if (!nav.empty()) {
        Xml navigation{ctx_};
        parseEntry(navigation, archive.p, nav);
        std::function<void(fz_xml*, int, bool)> walk = [&](fz_xml* n, int depth, bool toc) {
            if (depth > 32)
                return;
            for (; n && nav_.size() < 4096; n = fz_xml_next(n)) {
                std::string tag = fz_xml_tag(n) ? fz_xml_tag(n) : "";
                bool in = toc || (tag == "nav" && (std::string(att(n, "epub:type")).find("toc") !=
                                                       std::string::npos ||
                                                   std::string(att(n, "role")) == "doc-toc"));
                if (tag == "a" && in) {
                    std::string href = att(n, "href");
                    if (!href.empty() && href.find(':') == std::string::npos)
                        nav_.push_back({xmlText(fz_xml_down(n)), zipPath(nav, href), depth});
                }
                walk(fz_xml_down(n), depth + 1, in);
            }
        };
        walk(fz_xml_root(navigation.p), 0, false);
    }
    if (!cover.empty()) {
        auto existing =
            rootPath() + "/cache/covers/" + fingerprint(path) + "-source." + extension(cover);
        struct stat st{};
        if (!stat(existing.c_str(), &st)) {
            coverPath_ = existing;
            return path;
        }
        try {
            fz_buffer* b = readEntry(ctx_, archive.p, cover, 8 * 1024 * 1024);
            unsigned char* data = nullptr;
            size_t size = fz_buffer_storage(ctx_, b, &data);
            auto dest =
                rootPath() + "/cache/covers/" + fingerprint(path) + "-source." + extension(cover);
            std::string bytes;
            try {
                bytes.assign(reinterpret_cast<char*>(data), size);
            } catch (...) {
                fz_drop_buffer(ctx_, b);
                throw;
            }
            fz_drop_buffer(ctx_, b);
            if (atomicWrite(dest, bytes))
                coverPath_ = dest;
        } catch (...) { /* Cover is optional. */
        }
    }
    return path;
}
void EpubReader::extraContents() {
    if (nav_.empty())
        return;
    std::vector<Chapter> result;
    for (auto& link : nav_) {
        int page = -1;
        fz_try(ctx_) {
            page = fz_page_number_from_location(
                ctx_, doc_, fz_resolve_link(ctx_, doc_, link.uri.c_str(), nullptr, nullptr));
        }
        fz_catch(ctx_) {
            page = -1;
        }
        if (page >= 0)
            result.push_back({link.title, page, std::max(0, link.level - 4)});
    }
    if (!result.empty())
        chapters_ = std::move(result);
}
std::shared_ptr<Bitmap> EpubReader::cover() {
    if (!coverPath_.empty()) {
        MuReader image(cancel_);
        Settings s;
        s.fontSize = 20;
        s.orientation = 1;
        image.configure(s);
        if (image.open(coverPath_))
            return image.render(1);
    }
    return MuReader::cover();
}
} // namespace br
