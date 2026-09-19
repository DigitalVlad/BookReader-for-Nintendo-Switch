#pragma once
#include <string>
namespace br {
enum class Language { English, Russian };
std::string tr(const std::string& key, Language language);
std::string localizedError(const std::string& message, Language language);
} // namespace br
