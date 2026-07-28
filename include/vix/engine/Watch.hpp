/**
 *
 *  @file Watch.hpp
 *  @author Gaspard Kirira
 *
 *  Vix.cpp
 *
 *  Filesystem watch primitives
 *
 */

#ifndef VIX_ENGINE_WATCH_HPP
#define VIX_ENGINE_WATCH_HPP

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vix::engine::watch
{
  namespace fs = std::filesystem;

  enum class EventKind
  {
    Added,
    Modified,
    Removed,
    Renamed,
    Overflow
  };

  const char *to_string(EventKind kind);

  struct Event
  {
    EventKind kind{EventKind::Modified};
    fs::path path;
    fs::path oldPath;
    bool directory{false};
  };

  struct Batch
  {
    std::vector<Event> events;
    bool overflowed{false};

    bool empty() const;
  };

  struct Options
  {
    fs::path root;
    bool recursive{true};
    std::chrono::milliseconds debounce{std::chrono::milliseconds(25)};
    std::chrono::milliseconds maxBatchWindow{std::chrono::milliseconds(100)};
    std::chrono::milliseconds pollingInterval{std::chrono::milliseconds(100)};
    std::vector<fs::path> ignoredRoots;
  };

  struct Result
  {
    bool ok{false};
    std::string error;
    std::string backend;
  };

  class FileWatcher
  {
  public:
    explicit FileWatcher(Options options);
    ~FileWatcher();

    FileWatcher(const FileWatcher &) = delete;
    FileWatcher &operator=(const FileWatcher &) = delete;

    Result start();

    std::optional<Batch> wait_for_batch(
        std::chrono::milliseconds timeout);

    void stop();

    bool running() const;
    const Options &options() const;
    const std::string &backend() const;

  private:
    class Impl;

    Options options_;
    std::unique_ptr<Impl> impl_;
    std::string backend_;
  };

} // namespace vix::engine::watch

#endif
