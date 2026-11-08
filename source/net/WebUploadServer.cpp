#include "net/WebUploadServer.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <utility>
#include <vector>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace br {
namespace {

constexpr int kPort = 8080;
constexpr int64_t kMaxGlobal = 512LL * 1024 * 1024;
constexpr int64_t kMaxEpub = 128LL * 1024 * 1024;
constexpr int64_t kMaxFb2Html = 24LL * 1024 * 1024;
constexpr size_t kMaxHeaders = 16 * 1024;
constexpr size_t kMaxParts = 8;

bool sendAll(int fd, const char* data, size_t size) {
    while (size) {
#ifdef MSG_NOSIGNAL
        ssize_t n = send(fd, data, size, MSG_NOSIGNAL);
#else
        ssize_t n = send(fd, data, size, 0);
#endif
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        data += n;
        size -= size_t(n);
    }
    return true;
}

std::string lower(std::string s) {
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(std::string s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

std::string lowerExt(const std::string& name) {
    auto dot = name.find_last_of('.');
    if (dot == std::string::npos)
        return {};
    return lower(name.substr(dot + 1));
}

bool allowedExtension(const std::string& ext) {
    static const char* allowed[] = {"pdf", "epub", "txt", "fb2", "html", "htm", "cbz", "md"};
    for (auto* e : allowed)
        if (ext == e)
            return true;
    return false;
}

int64_t maxBytesFor(const std::string& ext) {
    if (ext == "epub")
        return kMaxEpub;
    if (ext == "fb2" || ext == "html" || ext == "htm")
        return kMaxFb2Html;
    return kMaxGlobal;
}

std::string htmlEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

std::string percentDecode(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'a' && c <= 'f')
                    return c - 'a' + 10;
                if (c >= 'A' && c <= 'F')
                    return c - 'A' + 10;
                return -1;
            };
            int a = hex(value[i + 1]), b = hex(value[i + 2]);
            if (a >= 0 && b >= 0) {
                out += char((a << 4) | b);
                i += 2;
                continue;
            }
        }
        out += value[i];
    }
    return out;
}

std::string sanitizeFilename(std::string name) {
    auto slash = name.find_last_of("/\\");
    if (slash != std::string::npos)
        name = name.substr(slash + 1);

    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|')
            out += '_';
        else
            out += char(c);
    }
    while (!out.empty() && (out.front() == '.' || out.front() == ' '))
        out.erase(out.begin());
    while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();

    auto dot = out.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : out.substr(dot);
    std::string stem = dot == std::string::npos ? out : out.substr(0, dot);
    if (stem.empty())
        stem = "book";
    while (stem.size() + ext.size() > 120 && !stem.empty())
        stem.pop_back();
    while (!stem.empty() && (static_cast<unsigned char>(stem.back()) & 0xC0) == 0x80)
        stem.pop_back();
    if (stem.empty())
        stem = "book";
    return stem + ext;
}

std::string uniqueName(const std::string& dir, const std::string& name) {
    if (access((dir + "/" + name).c_str(), F_OK) != 0)
        return name;
    auto dot = name.find_last_of('.');
    std::string stem = dot == std::string::npos ? name : name.substr(0, dot);
    std::string ext = dot == std::string::npos ? "" : name.substr(dot);
    for (int i = 1; i < 1000; ++i) {
        auto candidate = stem + "-" + std::to_string(i) + ext;
        if (access((dir + "/" + candidate).c_str(), F_OK) != 0)
            return candidate;
    }
    return stem + "-" + std::to_string(getpid()) + ext;
}

bool containsCI(const std::string& data, const std::string& needle) {
    return lower(data).find(lower(needle)) != std::string::npos;
}

bool validateFile(const std::string& path, const std::string& ext, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "Cannot read uploaded file";
        return false;
    }
    std::string data(4096, '\0');
    f.read(&data[0], std::streamsize(data.size()));
    data.resize(size_t(f.gcount()));

    if (ext == "pdf") {
        if (data.find("%PDF") == std::string::npos) {
            error = "The file is not a valid PDF";
            return false;
        }
    } else if (ext == "epub" || ext == "cbz") {
        if (data.size() < 2 || data[0] != 'P' || data[1] != 'K') {
            error = "The file is not a valid ZIP (" + ext + ")";
            return false;
        }
    } else if (ext == "fb2") {
        if (!containsCI(data, "FictionBook") && data.find('<') == std::string::npos) {
            error = "The file is not a valid FB2";
            return false;
        }
    } else if (ext == "html" || ext == "htm") {
        if (data.find('<') == std::string::npos) {
            error = "The file is not valid HTML";
            return false;
        }
    } else {
        bool utf16 = data.size() >= 2 && ((data[0] == '\xff' && data[1] == '\xfe') ||
                                          (data[0] == '\xfe' && data[1] == '\xff'));
        if (!utf16 && data.find('\0') != std::string::npos) {
            error = "The file is not a text file";
            return false;
        }
    }
    return true;
}

std::string boundaryFrom(const std::string& contentType) {
    auto pos = lower(contentType).find("boundary=");
    if (pos == std::string::npos)
        return {};
    auto start = pos + 9;
    if (start < contentType.size() && contentType[start] == '"') {
        auto end = contentType.find('"', start + 1);
        if (end == std::string::npos)
            return {};
        return contentType.substr(start + 1, end - start - 1);
    }
    auto end = contentType.find(';', start);
    return trim(contentType.substr(start, end == std::string::npos ? std::string::npos : end - start));
}

// Parses "form-data; name=\"file\"; filename=\"book.pdf\"" (and the RFC 5987
// filename* variant). Sets isFile when a filename parameter is present.
std::string filenameFromDisposition(const std::string& disposition, bool& isFile) {
    isFile = false;
    auto star = disposition.find("filename*=");
    if (star != std::string::npos) {
        isFile = true;
        auto start = star + 10;
        auto end = disposition.find(';', start);
        auto token = trim(disposition.substr(start, end == std::string::npos ? std::string::npos : end - start));
        auto quote = token.find("''");
        if (quote != std::string::npos)
            token = token.substr(quote + 2);
        return percentDecode(token);
    }
    auto pos = disposition.find("filename=");
    if (pos == std::string::npos)
        return {};
    isFile = true;
    auto start = pos + 9;
    if (start < disposition.size() && disposition[start] == '"') {
        ++start;
        std::string out;
        for (size_t i = start; i < disposition.size(); ++i) {
            char c = disposition[i];
            if (c == '"')
                break;
            if (c == '\\' && i + 1 < disposition.size())
                c = disposition[++i];
            out += c;
        }
        return out;
    }
    auto end = disposition.find(';', start);
    return trim(disposition.substr(start, end == std::string::npos ? std::string::npos : end - start));
}

// Streams the request body without ever keeping the whole file in memory.
class BodyReader {
  public:
    BodyReader(int fd, std::string prefix, int64_t remaining)
        : fd_(fd), buffer_(std::move(prefix)), remaining_(remaining) {}

    bool readExact(std::string& out, size_t size) {
        out.clear();
        while (out.size() < size) {
            if (buffer_.empty() && !fill(1))
                return false;
            size_t take = std::min(size - out.size(), buffer_.size());
            out.append(buffer_.data(), take);
            buffer_.erase(0, take);
        }
        return true;
    }

    bool readLine(std::string& line) {
        for (;;) {
            auto pos = buffer_.find("\r\n");
            if (pos != std::string::npos) {
                line = buffer_.substr(0, pos);
                buffer_.erase(0, pos + 2);
                return true;
            }
            auto lf = buffer_.find('\n');
            if (lf != std::string::npos) {
                line = buffer_.substr(0, lf);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                buffer_.erase(0, lf + 1);
                return true;
            }
            if (!fill(buffer_.size() + 1))
                return false;
        }
    }

    bool readUntil(const std::string& delimiter,
                   const std::function<bool(const char*, size_t)>& sink) {
        for (;;) {
            auto pos = buffer_.find(delimiter);
            if (pos != std::string::npos) {
                if (pos && !sink(buffer_.data(), pos))
                    return false;
                buffer_.erase(0, pos + delimiter.size());
                return true;
            }
            size_t keep = delimiter.empty() ? 0 : delimiter.size() - 1;
            if (buffer_.size() > keep) {
                size_t emit = buffer_.size() - keep;
                if (!sink(buffer_.data(), emit))
                    return false;
                buffer_.erase(0, emit);
            }
            if (!fill(buffer_.size() + 1))
                return false;
        }
    }

  private:
    bool fill(size_t minimum) {
        while (buffer_.size() < minimum) {
            if (remaining_ <= 0)
                return false;
            char chunk[65536];
            size_t want = size_t(std::min<int64_t>(int64_t(sizeof(chunk)), remaining_));
            ssize_t n = recv(fd_, chunk, want, 0);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                return false;
            }
            if (n == 0) {
                remaining_ = 0;
                return false;
            }
            buffer_.append(chunk, size_t(n));
            remaining_ -= n;
        }
        return true;
    }

    int fd_;
    std::string buffer_;
    int64_t remaining_;
};

struct MultipartResult {
    std::vector<std::string> files;
    std::string error;
};

bool parseMultipart(BodyReader& reader, const std::string& boundary, const std::string& booksDir,
                    MultipartResult& result) {
    const std::string first = "--" + boundary;
    const std::string separator = "\r\n--" + boundary;
    if (!reader.readUntil(first, [](const char*, size_t) { return true; })) {
        result.error = "Malformed upload";
        return false;
    }
    std::string two;
    if (!reader.readExact(two, 2)) {
        result.error = "Malformed upload";
        return false;
    }
    if (two == "--") {
        result.error = "No file uploaded";
        return false;
    }
    if (two != "\r\n") {
        result.error = "Malformed upload";
        return false;
    }

    for (size_t part = 0; part < kMaxParts; ++part) {
        std::string disposition, line;
        while (reader.readLine(line)) {
            if (line.empty())
                break;
            auto colon = line.find(':');
            if (colon == std::string::npos)
                continue;
            auto name = lower(trim(line.substr(0, colon)));
            auto value = trim(line.substr(colon + 1));
            if (name == "content-disposition")
                disposition = value;
        }

        bool isFile = false;
        auto filename = filenameFromDisposition(disposition, isFile);
        if (isFile && !filename.empty()) {
            auto ext = lowerExt(filename);
            if (!allowedExtension(ext)) {
                result.error = "Unsupported format: ." + ext;
                return false;
            }
            auto maximum = maxBytesFor(ext);
            auto temp = booksDir + "/.upload-" + std::to_string(getpid()) + "-" +
                        std::to_string(part) + ".part";
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            if (!file) {
                result.error = "Cannot write to microSD";
                return false;
            }
            int64_t written = 0;
            bool ok = reader.readUntil(separator, [&](const char* data, size_t size) {
                written += int64_t(size);
                if (written > maximum || written > kMaxGlobal) {
                    result.error = "File too large";
                    return false;
                }
                file.write(data, std::streamsize(size));
                if (!file) {
                    result.error = "Not enough space on microSD";
                    return false;
                }
                return true;
            });
            file.close();
            if (!ok) {
                remove(temp.c_str());
                if (result.error.empty())
                    result.error = "Upload interrupted";
                return false;
            }
            if (written == 0) {
                remove(temp.c_str());
                result.error = "Empty file";
                return false;
            }
            if (!validateFile(temp, ext, result.error)) {
                remove(temp.c_str());
                return false;
            }
            auto name = sanitizeFilename(filename);
            if (name.empty() || !allowedExtension(lowerExt(name))) {
                remove(temp.c_str());
                result.error = "Invalid file name";
                return false;
            }
            name = uniqueName(booksDir, name);
            auto destination = booksDir + "/" + name;
            if (rename(temp.c_str(), destination.c_str()) != 0) {
                remove(temp.c_str());
                result.error = "Cannot save the file";
                return false;
            }
            result.files.push_back(name);
        } else {
            if (!reader.readUntil(separator, [](const char*, size_t) { return true; })) {
                result.error = "Malformed upload";
                return false;
            }
        }

        if (!reader.readExact(two, 2)) {
            result.error = "Malformed upload";
            return false;
        }
        if (two == "--")
            return true;
        if (two != "\r\n") {
            result.error = "Malformed upload";
            return false;
        }
    }
    result.error = "Too many files in one upload";
    return false;
}

std::string page(const std::string& title, const std::string& body) {
    std::ostringstream html;
    html << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
         << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
         << "<title>" << htmlEscape(title) << "</title><style>"
         << ":root{color-scheme:light}*{box-sizing:border-box}"
         << "body{margin:0;background:#f6f7f9;color:#1f2731;"
            "font:16px/1.5 -apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif}"
         << "main{max-width:640px;margin:0 auto;padding:32px 20px 48px}"
         << "h1{font-size:28px;margin:0 0 4px}h2{font-size:22px;margin:0 0 8px}"
         << "p{margin:8px 0;color:#5c6a77}"
         << ".card{background:#fff;border:1px solid #e1e7ed;border-radius:14px;padding:22px;margin-top:20px}"
         << "label{display:block;font-weight:700;margin-bottom:8px}"
         << "input[type=file]{width:100%;padding:14px;border:2px dashed #b9c6d2;border-radius:10px;"
            "background:#f6f7f9;font-size:15px}"
         << "button{margin-top:16px;width:100%;padding:14px 18px;border:0;border-radius:10px;"
            "background:#217180;color:#fff;font-size:17px;font-weight:700;cursor:pointer}"
         << ".ok{background:#e7f5ec;border-color:#b7dfc3;color:#23633a}"
         << ".err{background:#fdecec;border-color:#f3c2c2;color:#8a2b2b}"
         << "ul{padding-left:20px}li{margin:4px 0}code{background:#eef2f5;padding:2px 6px;border-radius:6px}"
         << "a{color:#217180;font-weight:700}.muted{color:#5c6a77;font-size:14px}"
         << "</style></head><body><main><h1>BookReader</h1>"
         << "<p class=\"muted\">Upload books</p>" << body << "</main></body></html>";
    return html.str();
}

std::string formPage() {
    std::ostringstream body;
    body << "<div class=\"card\"><h2>Upload a book</h2>"
         << "<p>Open this page from your phone or PC on the same Wi-Fi network.</p>"
         << "<form action=\"/upload\" method=\"post\" enctype=\"multipart/form-data\">"
         << "<label for=\"file\">File</label>"
         << "<input id=\"file\" type=\"file\" name=\"file\" "
            "accept=\".pdf,.epub,.txt,.fb2,.html,.htm,.cbz,.md\" required>"
         << "<p class=\"muted\">PDF · EPUB · TXT · FB2 · HTML · CBZ · MD</p>"
         << "<button type=\"submit\">Upload</button></form></div>"
         << "<p class=\"muted\">Files are saved to "
            "<code>/switch/BookReader/books/</code>.</p>";
    return page("BookReader", body.str());
}

std::string successPage(const std::vector<std::string>& files) {
    std::ostringstream body;
    body << "<div class=\"card ok\"><h2>Done</h2><ul>";
    for (auto& file : files)
        body << "<li><strong>" << htmlEscape(file) << "</strong></li>";
    body << "</ul><p>The book is now in your library.</p></div>"
         << "<p><a href=\"/\">Upload another</a></p>";
    return page("BookReader — OK", body.str());
}

std::string errorPage(const std::string& message) {
    std::ostringstream body;
    body << "<div class=\"card err\"><h2>Error</h2><p>" << htmlEscape(message)
         << "</p></div><p><a href=\"/\">Back</a></p>";
    return page("BookReader — Error", body.str());
}

void respond(int fd, int code, const char* reason, const std::string& body) {
    std::ostringstream headers;
    headers << "HTTP/1.1 " << code << " " << reason << "\r\n"
            << "Content-Type: text/html; charset=utf-8\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n"
            << "Cache-Control: no-store\r\n\r\n";
    auto head = headers.str();
    sendAll(fd, head.data(), head.size());
    sendAll(fd, body.data(), body.size());
}

int statusForError(const std::string& error) {
    if (error.find("too large") != std::string::npos ||
        error.find("space") != std::string::npos)
        return 413;
    if (error.find("Unsupported") != std::string::npos)
        return 415;
    return 400;
}

void setTimeouts(int fd) {
    timeval timeout{};
    timeout.tv_sec = 60;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

std::string localIp() {
    std::string best;
    char host[256]{};
    if (gethostname(host, sizeof(host)) == 0) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        if (getaddrinfo(host, nullptr, &hints, &result) == 0) {
            for (auto* it = result; it; it = it->ai_next) {
                char text[INET_ADDRSTRLEN]{};
                auto* address = reinterpret_cast<sockaddr_in*>(it->ai_addr);
                if (!inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text)))
                    continue;
                std::string candidate(text);
                if (candidate == "127.0.0.1" || candidate.rfind("169.254.", 0) == 0) {
                    if (best.empty())
                        best = candidate;
                    continue;
                }
                best = candidate;
                break;
            }
            freeaddrinfo(result);
        }
    }
#ifdef __SWITCH__
    if (best.empty() || best == "127.0.0.1") {
        if (R_SUCCEEDED(nifmInitialize(NifmServiceType_User))) {
            u32 ip = 0;
            if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip) {
                auto textFor = [](u32 value) {
                    in_addr address{};
                    address.s_addr = value;
                    char text[INET_ADDRSTRLEN]{};
                    return inet_ntop(AF_INET, &address, text, sizeof(text)) ? std::string(text)
                                                                            : std::string();
                };
                auto isPrivate = [](const std::string& text) {
                    unsigned a = 0, b = 0, c = 0, d = 0;
                    if (std::sscanf(text.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
                        return false;
                    return a == 10 || (a == 192 && b == 168) ||
                           (a == 172 && b >= 16 && b <= 31) || (a == 169 && b == 254);
                };
                auto direct = textFor(ip);
                auto swapped = textFor(htonl(ip));
                if (isPrivate(direct))
                    best = direct;
                else if (isPrivate(swapped))
                    best = swapped;
                else if (!direct.empty() && direct != "0.0.0.0")
                    best = direct;
            }
            nifmExit();
        }
    }
#endif
    return best;
}

} // namespace

WebUploadServer::WebUploadServer(std::string booksDir) : booksDir_(std::move(booksDir)) {
}

WebUploadServer::~WebUploadServer() {
    stop();
}

bool WebUploadServer::start(std::string& error) {
    if (active_.load())
        return true;

#ifdef SIGPIPE
    std::signal(SIGPIPE, SIG_IGN);
#endif

    stop_.store(false);
#ifdef __SWITCH__
    if (R_FAILED(socketInitializeDefault())) {
        error = "Connect to Wi-Fi first";
        return false;
    }
    socketReady_.store(true);
#endif

    url_.clear();
    auto ip = localIp();
    if (ip.empty()) {
#ifdef __SWITCH__
        if (socketReady_.exchange(false))
            socketExit();
        error = "Connect to Wi-Fi first";
#else
        ip = "127.0.0.1";
#endif
#ifndef __SWITCH__
        // Host builds still allow local testing.
#endif
    }
    if (ip.empty()) {
#ifdef __SWITCH__
        if (socketReady_.exchange(false))
            socketExit();
#endif
        error = "Connect to Wi-Fi first";
        return false;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = std::string("Cannot create socket: ") + std::strerror(errno);
#ifdef __SWITCH__
        if (socketReady_.exchange(false))
            socketExit();
#endif
        return false;
    }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(kPort);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        error = errno == EADDRINUSE ? "Port 8080 is already in use"
                                    : std::string("Cannot bind port 8080: ") + std::strerror(errno);
        close(fd);
#ifdef __SWITCH__
        if (socketReady_.exchange(false))
            socketExit();
#endif
        return false;
    }
    if (listen(fd, 4) != 0) {
        error = std::string("Cannot listen: ") + std::strerror(errno);
        close(fd);
#ifdef __SWITCH__
        if (socketReady_.exchange(false))
            socketExit();
#endif
        return false;
    }

    listen_.store(fd);
    url_ = "http://" + ip + ":" + std::to_string(kPort);
    active_.store(true);
    thread_ = std::thread([this] { run(); });
    return true;
}

void WebUploadServer::stop() {
    stop_.store(true);
    int listener = listen_.load();
    if (listener >= 0)
        shutdown(listener, SHUT_RDWR);
    int connection = client_.load();
    if (connection >= 0)
        shutdown(connection, SHUT_RDWR);
    if (thread_.joinable())
        thread_.join();
    active_.store(false);
}

void WebUploadServer::run() {
    while (!stop_.load()) {
        int listener = listen_.load();
        if (listener < 0)
            break;
        pollfd poller{};
        poller.fd = listener;
        poller.events = POLLIN;
        int ready = poll(&poller, 1, 500);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (ready == 0 || !(poller.revents & POLLIN))
            continue;

        sockaddr_in peer{};
        socklen_t length = sizeof(peer);
        int connection = accept(listener, reinterpret_cast<sockaddr*>(&peer), &length);
        if (connection < 0) {
            if (errno == EINTR || errno == EAGAIN || stop_.load())
                continue;
            continue;
        }
        client_.store(connection);
        setTimeouts(connection);
        handleClient(connection);
        client_.store(-1);
        close(connection);
    }

    int listener = listen_.exchange(-1);
    if (listener >= 0)
        close(listener);
    active_.store(false);
#ifdef __SWITCH__
    if (socketReady_.exchange(false))
        socketExit();
#endif
}

void WebUploadServer::handleClient(int fd) {
    std::string raw;
    char chunk[8192];
    while (raw.find("\r\n\r\n") == std::string::npos && raw.size() < kMaxHeaders) {
        ssize_t size = recv(fd, chunk, sizeof(chunk), 0);
        if (size < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        if (size == 0)
            return;
        raw.append(chunk, size_t(size));
    }

    auto headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        respond(fd, 431, "Request Header Fields Too Large", errorPage("Bad request"));
        return;
    }
    std::string head = raw.substr(0, headerEnd);
    std::string bodyPrefix = raw.substr(headerEnd + 4);

    std::istringstream lines(head);
    std::string requestLine;
    if (!std::getline(lines, requestLine))
        return;
    if (!requestLine.empty() && requestLine.back() == '\r')
        requestLine.pop_back();

    std::istringstream request(requestLine);
    std::string method, path, version;
    request >> method >> path >> version;
    (void)version;
    auto query = path.find('?');
    if (query != std::string::npos)
        path = path.substr(0, query);

    std::string contentType, expect, transferEncoding;
    int64_t contentLength = -1;
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        auto name = lower(trim(line.substr(0, colon)));
        auto value = trim(line.substr(colon + 1));
        if (name == "content-type")
            contentType = value;
        else if (name == "content-length")
            contentLength = std::strtoll(value.c_str(), nullptr, 10);
        else if (name == "expect")
            expect = lower(value);
        else if (name == "transfer-encoding")
            transferEncoding = lower(value);
    }

    if (method == "GET" && (path == "/" || path == "/index.html")) {
        respond(fd, 200, "OK", formPage());
        return;
    }
    if (method == "GET") {
        respond(fd, 404, "Not Found", errorPage("Not found"));
        return;
    }
    if (method != "POST" || path != "/upload") {
        respond(fd, 405, "Method Not Allowed", errorPage("Method not allowed"));
        return;
    }
    if (!transferEncoding.empty() || contentLength < 0) {
        respond(fd, 411, "Length Required",
                errorPage("Content-Length is required for uploads"));
        return;
    }
    if (contentLength == 0) {
        respond(fd, 400, "Bad Request", errorPage("No file uploaded"));
        return;
    }
    if (contentLength > kMaxGlobal + 1024 * 1024) {
        respond(fd, 413, "Payload Too Large", errorPage("File too large"));
        return;
    }
    if (expect.find("100-continue") != std::string::npos) {
        static const char go[] = "HTTP/1.1 100 Continue\r\n\r\n";
        sendAll(fd, go, sizeof(go) - 1);
    }

    auto boundary = boundaryFrom(contentType);
    if (boundary.empty()) {
        respond(fd, 400, "Bad Request", errorPage("Invalid multipart request"));
        return;
    }
    if (bodyPrefix.size() > size_t(contentLength))
        bodyPrefix.resize(size_t(contentLength));
    size_t prefixSize = bodyPrefix.size();
    BodyReader reader(fd, std::move(bodyPrefix), contentLength - int64_t(prefixSize));
    MultipartResult result;
    if (!parseMultipart(reader, boundary, booksDir_, result)) {
        respond(fd, statusForError(result.error), "Bad Request", errorPage(result.error));
        return;
    }
    if (result.files.empty()) {
        respond(fd, 400, "Bad Request", errorPage("No file uploaded"));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(uploadMutex_);
        uploadSummary_ = result.files.size() == 1
                             ? result.files.front()
                             : std::to_string(result.files.size()) + " files";
        hasUpload_ = true;
    }
    respond(fd, 200, "OK", successPage(result.files));
}

bool WebUploadServer::takeUpload(std::string& summary) {
    std::lock_guard<std::mutex> lock(uploadMutex_);
    if (!hasUpload_)
        return false;
    summary = uploadSummary_;
    uploadSummary_.clear();
    hasUpload_ = false;
    return true;
}

} // namespace br
