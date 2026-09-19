#include "readers/MuReader.hpp"
#include "utils/Files.hpp"
#include <map>
#include <stdexcept>
namespace br {
static std::string textOf(fz_xml* n, int depth = 0) {
    if (depth > 64)
        throw std::runtime_error("FB2 nesting limit");
    std::string s;
    for (; n; n = fz_xml_next(n)) {
        if (auto* t = fz_xml_text(n))
            s += t;
        else
            s += textOf(fz_xml_down(n), depth + 1);
        if (s.size() > 24 * 1024 * 1024)
            throw std::runtime_error("FB2 text limit");
    }
    return s;
}
static void htmlOf(fz_xml* n, std::string& out, const std::map<std::string, std::string>& images,
                   int depth = 0) {
    if (depth > 64)
        throw std::runtime_error("FB2 nesting limit");
    for (; n; n = fz_xml_next(n)) {
        if (auto* t = fz_xml_text(n)) {
            out += htmlEscape(t);
            continue;
        }
        std::string tag = fz_xml_tag(n) ? fz_xml_tag(n) : "";
        if (tag == "binary" || tag == "description")
            continue;
        std::string h = tag == "title"             ? "h2"
                        : tag == "subtitle"        ? "h3"
                        : tag == "strong"          ? "strong"
                        : tag == "emphasis"        ? "em"
                        : tag == "strikethrough"   ? "s"
                        : tag == "p" || tag == "v" ? "p"
                        : tag == "empty-line"      ? "br"
                                                   : "div";
        out += "<" + h + ">";
        if (tag == "image") {
            const char* href = fz_xml_att(n, "xlink:href");
            if (!href)
                href = fz_xml_att(n, "l:href");
            if (!href)
                href = fz_xml_att(n, "href");
            auto it = images.find(href ? href : "");
            if (it != images.end())
                out += "<img src=\"" + it->second + "\"/>";
            else
                out += "[Unsupported or missing FB2 image]";
        } else
            htmlOf(fz_xml_down(n), out, images, depth + 1);
        out += "</" + h + ">";
        if (out.size() > 32 * 1024 * 1024)
            throw std::runtime_error("FB2 output limit");
    }
}
std::string Fb2Reader::prepare(const std::string& path) {
    fz_buffer* buf = nullptr;
    fz_xml_doc* xml = nullptr;
    fz_var(buf);
    fz_var(xml);
    bool ok = true;
    char msg[256]{};
    fz_try(ctx_) {
        buf = fz_read_file(ctx_, path.c_str());
        xml = fz_parse_xml(ctx_, buf, 1, 0);
    }
    fz_always(ctx_) {
        fz_drop_buffer(ctx_, buf);
    }
    fz_catch(ctx_) {
        ok = false;
        snprintf(msg, sizeof(msg), "%s", fz_caught_message(ctx_));
    }
    if (!ok) {
        fz_drop_xml(ctx_, xml);
        throw std::runtime_error(std::string("Invalid FB2: ") + msg);
    }
    std::string result;
    try {
        auto* root = fz_xml_root(xml);
        if (!fz_xml_is_tag(root, "FictionBook"))
            throw std::runtime_error("Invalid FB2 root");
        auto* desc = fz_xml_find_down(root, "description");
        auto* info = desc ? fz_xml_find_down(desc, "title-info") : nullptr;
        if (info) {
            auto* t = fz_xml_find_down(info, "book-title");
            if (t)
                title_ = textOf(fz_xml_down(t));
            auto* a = fz_xml_find_down(info, "author");
            if (a) {
                for (auto* c = fz_xml_down(a); c; c = fz_xml_next(c)) {
                    if (!author_.empty())
                        author_ += ' ';
                    author_ += textOf(fz_xml_down(c));
                }
            }
        }
        std::map<std::string, std::string> images;
        size_t total = 0;
        for (auto* n = fz_xml_find_down(root, "binary"); n; n = fz_xml_find_next(n, "binary")) {
            const char *id = fz_xml_att(n, "id"), *mime = fz_xml_att(n, "content-type");
            if (!id || !mime)
                continue;
            std::string type = mime;
            if (type != "image/jpeg" && type != "image/png")
                continue;
            auto data = textOf(fz_xml_down(n));
            if (data.size() > 8 * 1024 * 1024 || total + data.size() > 12 * 1024 * 1024)
                continue;
            total += data.size();
            std::string clean;
            for (unsigned char c : data)
                if (std::isalnum(c) || c == '+' || c == '/' || c == '=')
                    clean += char(c);
            images[std::string("#") + id] = "data:" + type + ";base64," + clean;
        }
        result = "<html><head><meta charset=\"utf-8\"><title>" + htmlEscape(title_) +
                 "</title></head><body>";
        for (auto* body = fz_xml_find_down(root, "body"); body;
             body = fz_xml_find_next(body, "body"))
            htmlOf(fz_xml_down(body), result, images);
        result += "</body></html>";
    } catch (...) {
        fz_drop_xml(ctx_, xml);
        throw;
    }
    fz_drop_xml(ctx_, xml);
    auto output = rootPath() + "/cache/fb2/" + fingerprint(path) + ".html";
    if (!atomicWrite(output, result))
        throw std::runtime_error("Cannot write FB2 cache");
    trimCache(rootPath() + "/cache/fb2", 64 * 1024 * 1024);
    return output;
}
} // namespace br
