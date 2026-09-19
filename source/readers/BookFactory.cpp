#include "readers/BookFactory.hpp"
#include "readers/MuReader.hpp"
#include "readers/TxtReader.hpp"
#include "utils/Files.hpp"
#include <stdexcept>
namespace br {
std::unique_ptr<IBookReader> BookFactory::open(const std::string& p, const Settings& s,
                                               const std::vector<FontSource>& fonts,
                                               std::atomic<bool>& cancel) {
    std::unique_ptr<IBookReader> r;
    auto e = extension(p);
    if (e == "txt" || e == "md")
        r = std::make_unique<TxtReader>(fonts, cancel);
    else if (e == "pdf")
        r = std::make_unique<PdfReader>(cancel);
    else if (e == "epub")
        r = std::make_unique<EpubReader>(cancel);
    else if (e == "fb2")
        r = std::make_unique<Fb2Reader>(cancel);
    else if (e == "html" || e == "htm")
        r = std::make_unique<HtmlReader>(cancel);
    else if (e == "cbz")
        r = std::make_unique<CbzReader>(cancel);
    else
        throw std::runtime_error("Unsupported format");
    r->configure(s);
    if (!r->open(p))
        throw std::runtime_error(r->error());
    return r;
}
} // namespace br
