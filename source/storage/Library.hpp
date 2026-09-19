#pragma once
#include "utils/Types.hpp"
#include <atomic>
namespace br {
std::vector<Book> scanLibrary(const std::string& directory, const std::atomic<bool>& cancel);
bool supported(const std::string& ext);
void sortLibrary(std::vector<Book>& books, int sort);
} // namespace br
