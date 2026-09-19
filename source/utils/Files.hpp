#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace br {
std::string rootPath();
bool ensureDirectories(const std::string& root);
std::string extension(const std::string& path);
std::string basename(const std::string& path);
std::string keyFor(const std::string& path);
std::string escape(const std::string& value);
std::string unescape(const std::string& value);
std::string htmlEscape(const std::string& value);
std::string fold(const std::string& value);
std::string fingerprint(const std::string& path);
bool atomicWrite(const std::string& path, const std::string& data);
void trimCache(const std::string& directory, uint64_t budget);
uint32_t utf8Next(const std::string& s, size_t& i);
std::string utf8(uint32_t cp);
} // namespace br
