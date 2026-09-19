#include "storage/Library.hpp"
#include "utils/Files.hpp"
#include <dirent.h>
#include <sys/stat.h>
namespace br {
bool supported(const std::string& e) {
    return e == "pdf" || e == "epub" || e == "txt" || e == "fb2" || e == "html" || e == "htm" ||
           e == "cbz" || e == "md";
}
static void scan(const std::string& p, std::vector<Book>& books, int depth,
                 const std::atomic<bool>& cancel) {
    if (depth > 32 || cancel || books.size() >= 10000)
        return;
    DIR* d = opendir(p.c_str());
    if (!d)
        return;
    while (auto* e = readdir(d)) {
        if (cancel || books.size() >= 10000)
            break;
        if (e->d_name[0] == '.')
            continue;
        std::string path = p + "/" + e->d_name;
        struct stat s{};
        if (lstat(path.c_str(), &s))
            continue;
        if (S_ISDIR(s.st_mode))
            scan(path, books, depth + 1, cancel);
        else if (S_ISREG(s.st_mode) && supported(extension(path))) {
            Book b;
            b.path = path;
            b.title = basename(path);
            b.format = extension(path);
            b.modified = s.st_mtime;
            books.push_back(b);
        }
    }
    closedir(d);
}
std::vector<Book> scanLibrary(const std::string& p, const std::atomic<bool>& cancel) {
    std::vector<Book> b;
    scan(p, b, 0, cancel);
    sortLibrary(b, 0);
    return b;
}
void sortLibrary(std::vector<Book>& b, int mode) {
    std::stable_sort(b.begin(), b.end(), [mode](const Book& a, const Book& b) {
        if (mode == 2 && a.modified != b.modified)
            return a.modified > b.modified;
        if (mode == 4 && a.opened != b.opened)
            return a.opened > b.opened;
        auto aa = fold(mode == 1   ? a.author
                       : mode == 3 ? a.format
                                   : a.title),
             bb = fold(mode == 1   ? b.author
                       : mode == 3 ? b.format
                                   : b.title);
        return aa == bb ? fold(a.title) < fold(b.title) : aa < bb;
    });
}
} // namespace br
