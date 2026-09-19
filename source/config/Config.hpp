#pragma once
#include "utils/Types.hpp"
#include <chrono>
#include <map>
namespace br {
class Config {
    std::string path_;
    bool dirty_ = false;
    double firstDirty_ = 0, lastDirty_ = 0, lastSave_ = 0;
    bool parse(const std::string& path);

  public:
    Settings settings;
    std::map<std::string, Book> books;
    std::string lastBook;
    explicit Config(std::string path) : path_(std::move(path)) {}
    bool load();
    void changed(double now);
    bool save(double now, bool force = false);
    void merge(std::vector<Book>& library) const;
};
double monotonic();
} // namespace br
