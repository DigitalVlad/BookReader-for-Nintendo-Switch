#pragma once
#include "readers/IBookReader.hpp"
#include "render/Fonts.hpp"
namespace br {
class BookFactory {
  public:
    static std::unique_ptr<IBookReader> open(const std::string& path, const Settings& settings,
                                             const std::vector<FontSource>& fonts,
                                             std::atomic<bool>& cancel);
};
} // namespace br
