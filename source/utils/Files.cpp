#include "utils/Files.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
namespace br {
std::string rootPath() {
#ifdef __SWITCH__
    return "sdmc:/switch/BookReader";
#else
    const char* p = std::getenv("BOOKREADER_ROOT");
    return p ? p : "sdcard/switch/BookReader";
#endif
}
bool ensureDirectories(const std::string& root) {
    for (size_t i = 1; i <= root.size(); ++i)
        if (i == root.size() || root[i] == '/') {
            auto p = root.substr(0, i);
            if (p.back() != ':')
                mkdir(p.c_str(), 0777);
        }
    for (auto sub :
         {"books", "fonts", "config", "cache", "cache/covers", "cache/text", "cache/fb2"}) {
        auto p = root + "/" + sub;
        mkdir(p.c_str(), 0777);
        struct stat st{};
        if (stat(p.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
            return false;
    }
    return true;
}
std::string extension(const std::string& p) {
    auto n = p.find_last_of('.');
    if (n == std::string::npos)
        return {};
    auto s = p.substr(n + 1);
    for (auto& c : s)
        c = char(std::tolower((unsigned char)c));
    return s;
}
std::string basename(const std::string& p) {
    auto n = p.find_last_of('/');
    return p.substr(n == std::string::npos ? 0 : n + 1);
}
std::string keyFor(const std::string& p) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : p) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << h;
    return ss.str();
}
std::string escape(const std::string& s) {
    std::string r;
    const char* hex = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if (c < 32 || c == '%' || c == '=' || c == '[' || c == ']') {
            r += '%';
            r += hex[c >> 4];
            r += hex[c & 15];
        } else
            r += char(c);
    }
    return r;
}
std::string unescape(const std::string& s) {
    std::string r;
    auto hex = [](char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    };
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && hex(s[i + 1]) >= 0 && hex(s[i + 2]) >= 0) {
            r += char(hex(s[i + 1]) * 16 + hex(s[i + 2]));
            i += 2;
        } else
            r += s[i];
    }
    return r;
}
std::string htmlEscape(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
        case '&':
            r += "&amp;";
            break;
        case '<':
            r += "&lt;";
            break;
        case '>':
            r += "&gt;";
            break;
        case '"':
            r += "&quot;";
            break;
        default:
            r += c;
        }
    }
    return r;
}
uint32_t utf8Next(const std::string& s, size_t& i) {
    if (i >= s.size())
        return 0;
    uint32_t c = (unsigned char)s[i++];
    if (c < 128)
        return c;
    int n = c >= 0xC2 && c <= 0xDF   ? 1
            : c >= 0xE0 && c <= 0xEF ? 2
            : c >= 0xF0 && c <= 0xF4 ? 3
                                     : 0;
    if (!n)
        return 0xFFFD;
    uint32_t v = c & ((1 << (6 - n)) - 1);
    size_t start = i;
    for (int j = 0; j < n; ++j) {
        if (i >= s.size() || ((unsigned char)s[i] & 0xC0) != 0x80) {
            i = start;
            return 0xFFFD;
        }
        v = (v << 6) | ((unsigned char)s[i++] & 63);
    }
    if (v < (n == 1 ? 128 : n == 2 ? 2048 : 65536) || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF))
        return 0xFFFD;
    return v;
}
std::string utf8(uint32_t c) {
    if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
        c = 0xFFFD;
    std::string s;
    if (c < 128)
        s += char(c);
    else if (c < 2048) {
        s += char(0xC0 | (c >> 6));
        s += char(0x80 | (c & 63));
    } else if (c < 65536) {
        s += char(0xE0 | (c >> 12));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    } else {
        s += char(0xF0 | (c >> 18));
        s += char(0x80 | ((c >> 12) & 63));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    }
    return s;
}
std::string fold(const std::string& s) {
    std::string r;
    size_t i = 0;
    while (i < s.size()) {
        auto c = utf8Next(s, i);
        if (c >= 'A' && c <= 'Z')
            c += 32;
        if (c >= 0x410 && c <= 0x42F)
            c += 32;
        if (c == 0x401)
            c = 0x451;
        r += utf8(c);
    }
    return r;
}
std::string fingerprint(const std::string& p) {
    struct stat s{};
    if (stat(p.c_str(), &s))
        return {};
    return keyFor(p + ":" + std::to_string(s.st_size) + ":" + std::to_string(s.st_mtime));
}
bool atomicWrite(const std::string& p, const std::string& data) {
    auto tmp = p + ".tmp", bak = p + ".bak";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f)
        return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    if (fflush(f))
        ok = false;
    if (fsync(fileno(f)))
        ok = false;
    if (fclose(f))
        ok = false;
    if (!ok) {
        remove(tmp.c_str());
        return false;
    }
    // Retain the last good generation; recovery reads .bak if primary is absent/truncated.
    remove(bak.c_str());
    rename(p.c_str(), bak.c_str());
    if (rename(tmp.c_str(), p.c_str()) != 0) {
        rename(bak.c_str(), p.c_str());
        return false;
    }
    return true;
}
void trimCache(const std::string& p, uint64_t budget) {
    struct Item {
        std::string p;
        uint64_t size;
        int64_t t;
    };
    std::vector<Item> items;
    uint64_t total = 0;
    DIR* d = opendir(p.c_str());
    if (!d)
        return;
    while (auto* e = readdir(d)) {
        if (e->d_name[0] == '.')
            continue;
        std::string f = p + "/" + e->d_name;
        struct stat s{};
        if (!lstat(f.c_str(), &s) && S_ISREG(s.st_mode)) {
            items.push_back({f, uint64_t(s.st_size), s.st_mtime});
            total += s.st_size;
        }
    }
    closedir(d);
    std::sort(items.begin(), items.end(), [](auto& a, auto& b) { return a.t < b.t; });
    for (auto& item : items) {
        if (total <= budget)
            break;
        if (!remove(item.p.c_str()))
            total -= item.size;
    }
}
} // namespace br
