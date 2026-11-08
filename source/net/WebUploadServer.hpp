#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace br {

// Minimal HTTP upload server for the BookReader books folder.
//
// It is intentionally dependency-free: POSIX sockets are provided by libnx on
// the Switch and by the host when running the test build. The server streams
// multipart uploads straight to disk (never into RAM), validates the extension
// and the file signature, and writes atomically through a hidden .part file.
class WebUploadServer {
  public:
    explicit WebUploadServer(std::string booksDir);
    ~WebUploadServer();
    WebUploadServer(const WebUploadServer&) = delete;
    WebUploadServer& operator=(const WebUploadServer&) = delete;

    // Starts listening on 0.0.0.0:8080. Returns false and fills error on
    // failure (no Wi-Fi, port busy, socket init failure...).
    bool start(std::string& error);
    void stop();
    bool active() const { return active_.load(); }
    std::string url() const { return url_; }

    // Consumes the "a file was uploaded" notification. Returns true once per
    // upload batch and writes a short human readable summary.
    bool takeUpload(std::string& summary);

  private:
    void run();
    void handleClient(int fd);

    std::string booksDir_;
    std::string url_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> active_{false};
    std::atomic<int> listen_{-1};
    std::atomic<int> client_{-1};
    std::atomic<bool> socketReady_{false};

    std::mutex uploadMutex_;
    std::string uploadSummary_;
    bool hasUpload_ = false;
};

} // namespace br
