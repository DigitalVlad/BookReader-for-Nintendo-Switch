#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
namespace br {
// Each MuPDF context owns one allocator; accessed only by its reader worker.
class MemoryManager {
    struct alignas(std::max_align_t) Header {
        size_t size;
    };
    size_t used_ = 0, peak_ = 0, limit_;

  public:
    explicit MemoryManager(size_t limit = 160 * 1024 * 1024) : limit_(limit) {}
    static void* allocate(void* user, size_t size) {
        auto& m = *static_cast<MemoryManager*>(user);
        if (size > m.limit_ - m.used_ || size > SIZE_MAX - sizeof(Header))
            return nullptr;
        auto* h = static_cast<Header*>(std::malloc(sizeof(Header) + size));
        if (!h)
            return nullptr;
        h->size = size;
        m.used_ += size;
        m.peak_ = std::max(m.peak_, m.used_);
        return h + 1;
    }
    static void release(void* user, void* p) {
        if (!p)
            return;
        auto& m = *static_cast<MemoryManager*>(user);
        auto* h = static_cast<Header*>(p) - 1;
        m.used_ -= h->size;
        std::free(h);
    }
    static void* resize(void* user, void* p, size_t size) {
        if (!p)
            return allocate(user, size);
        if (!size) {
            release(user, p);
            return nullptr;
        }
        auto& m = *static_cast<MemoryManager*>(user);
        auto* h = static_cast<Header*>(p) - 1;
        if (size > m.limit_ - (m.used_ - h->size) || size > SIZE_MAX - sizeof(Header))
            return nullptr;
        size_t old = h->size;
        auto* n = static_cast<Header*>(std::realloc(h, sizeof(Header) + size));
        if (!n)
            return nullptr;
        n->size = size;
        m.used_ = m.used_ - old + size;
        m.peak_ = std::max(m.peak_, m.used_);
        return n + 1;
    }
    size_t used() const { return used_; }
    size_t peak() const { return peak_; }
};
} // namespace br
