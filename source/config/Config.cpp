#include "config/Config.hpp"
#include "utils/Files.hpp"
#include <cmath>
#include <fstream>
#include <sstream>
namespace br {
double monotonic() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
static double number(const std::string& s, double fallback) {
    try {
        size_t end = 0;
        double v = std::stod(s, &end);
        return end == s.size() && std::isfinite(v) && std::abs(v) < 1e15 ? v : fallback;
    } catch (...) {
        return fallback;
    }
}
static int integer(const std::string& s, int fallback) {
    return int(std::clamp(number(s, fallback), -100000000., 100000000.));
}
static void setting(Settings& s, const std::string& k, const std::string& v) {
    if (k == "language")
        s.language = v == "ru" ? Language::Russian : Language::English;
    if (k == "fontSize")
        s.fontSize = integer(v, 28);
    if (k == "margin")
        s.margin = integer(v, 40);
    if (k == "theme")
        s.theme = integer(v, 0);
    if (k == "orientation")
        s.orientation = v == "portrait" ? 1 : v == "landscape" ? 0 : integer(v, 0);
    if (k == "font")
        s.font = integer(v, 0);
    if (k == "alignment")
        s.alignment = integer(v, 0);
    if (k == "lineSpacing")
        s.lineSpacing = float(number(v, 1.5));
    if (k == "brightness")
        s.brightness = float(number(v, 1));
    if (k == "scrollMode")
        s.scrollMode = integer(v, 0) != 0;
    s.clamp();
}
static void writeSettings(std::ostream& o, const Settings& s) {
    o << "fontSize=" << s.fontSize << "\nmargin=" << s.margin << "\ntheme=" << s.theme
      << "\norientation=" << (s.orientation ? "portrait" : "landscape")
      << "\nlanguage=" << (s.language == Language::Russian ? "ru" : "en") << "\nfont=" << s.font
      << "\nalignment=" << s.alignment << "\nlineSpacing=" << s.lineSpacing
      << "\nbrightness=" << s.brightness << "\nscrollMode=" << s.scrollMode << '\n';
}
bool Config::parse(const std::string& p) {
    std::ifstream f(p);
    if (!f)
        return false;
    f.seekg(0, std::ios::end);
    if (f.tellg() > 8 * 1024 * 1024)
        return false;
    f.seekg(0);
    std::string line, section;
    Book b;
    bool header = false, complete = false;
    std::map<std::string, Book> parsed;
    Settings defaults;
    std::string last;
    auto finish = [&] {
        if (!b.path.empty()) {
            parsed[b.path] = b;
            b = {};
        }
    };
    while (std::getline(f, line)) {
        if (line.size() > 16384)
            return false;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line == "; BookReader 1")
            header = true;
        if (line == "; complete")
            complete = true;
        if (line.size() > 1 && line.front() == '[' && line.back() == ']') {
            finish();
            section = line.substr(1, line.size() - 2);
            continue;
        }
        auto n = line.find('=');
        if (n == std::string::npos)
            continue;
        auto k = line.substr(0, n), v = unescape(line.substr(n + 1));
        if (section == "settings") {
            setting(defaults, k, v);
            if (k == "lastBook")
                last = v;
        } else if (section.rfind("book:", 0) == 0) {
            setting(b.settings, k, v);
            if (k == "path")
                b.path = v;
            if (k == "title")
                b.title = v;
            if (k == "author")
                b.author = v;
            if (k == "favorite")
                b.favorite = integer(v, 0) != 0;
            if (k == "opened")
                b.opened = int64_t(std::max(0., number(v, 0)));
            if (k == "page")
                b.position.page = std::max(0, integer(v, 0));
            if (k == "chapter")
                b.position.chapter = std::max(0, integer(v, 0));
            if (k == "offset")
                b.position.offset = uint64_t(std::max(0., number(v, 0)));
            if (k == "zoom")
                b.position.zoom = float(std::clamp(number(v, 1), 1., 4.));
            if (k == "panX")
                b.position.panX = float(std::clamp(number(v, 0), -10000., 10000.));
            if (k == "panY")
                b.position.panY = float(std::clamp(number(v, 0), -10000., 10000.));
            if (k == "progress")
                b.position.progress = float(std::clamp(number(v, 0), 0., 1.));
            if (k == "hasSettings")
                b.hasSettings = integer(v, 0) != 0;
        }
    }
    finish();
    if (!header || !complete)
        return false;
    settings = defaults;
    lastBook = last;
    books = std::move(parsed);
    return true;
}
bool Config::load() {
    return parse(path_) || parse(path_ + ".bak");
}
void Config::changed(double now) {
    if (!dirty_)
        firstDirty_ = now;
    dirty_ = true;
    lastDirty_ = now;
}
bool Config::save(double now, bool force) {
    if (!dirty_)
        return true;
    if (!force && ((now - lastDirty_ < 2 && now - firstDirty_ < 20) || now - lastSave_ < 5))
        return true;
    std::ostringstream o;
    o << "; BookReader 1\n[settings]\nlastBook=" << escape(lastBook) << '\n';
    writeSettings(o, settings);
    for (auto& entry : books) {
        const auto& b = entry.second;
        o << "\n[book:" << keyFor(b.path) << "]\npath=" << escape(b.path)
          << "\ntitle=" << escape(b.title) << "\nauthor=" << escape(b.author)
          << "\nfavorite=" << b.favorite << "\nopened=" << b.opened << "\npage=" << b.position.page
          << "\nchapter=" << b.position.chapter << "\noffset=" << b.position.offset
          << "\nzoom=" << b.position.zoom << "\npanX=" << b.position.panX
          << "\npanY=" << b.position.panY << "\nprogress=" << b.position.progress
          << "\nhasSettings=" << b.hasSettings << '\n';
        writeSettings(o, b.settings);
    }
    o << "; complete\n";
    if (!atomicWrite(path_, o.str()))
        return false;
    dirty_ = false;
    lastSave_ = now;
    return true;
}
void Config::merge(std::vector<Book>& library) const {
    for (auto& b : library) {
        auto i = books.find(b.path);
        if (i != books.end()) {
            auto path = b.path, format = b.format;
            auto modified = b.modified;
            b = i->second;
            b.path = path;
            b.format = format;
            b.modified = modified;
            if (b.title.empty())
                b.title = basename(path);
        }
    }
}
} // namespace br
