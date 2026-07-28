#include <vix/engine/Watch.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
namespace watch = vix::engine::watch;

namespace
{
  struct TempDir
  {
    fs::path path;

    TempDir()
    {
      path = fs::temp_directory_path() /
             ("vix-engine-watch-tests-" +
              std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count()));
      fs::create_directories(path);
    }

    ~TempDir()
    {
      std::error_code ec;
      fs::remove_all(path, ec);
    }
  };

  static void write_file(const fs::path &path, const std::string &content)
  {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
  }

  static bool has_path(const watch::Batch &batch, const fs::path &path);

  static watch::Batch wait_non_empty(watch::FileWatcher &w)
  {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);

    while (std::chrono::steady_clock::now() < deadline)
    {
      auto batch = w.wait_for_batch(std::chrono::milliseconds(250));
      if (batch && !batch->empty())
        return *batch;
    }

    assert(false && "timed out waiting for watch event");
    return {};
  }

  static watch::Batch wait_for_path(watch::FileWatcher &w, const fs::path &path)
  {
    watch::Batch combined;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);

    while (std::chrono::steady_clock::now() < deadline)
    {
      auto batch = w.wait_for_batch(std::chrono::milliseconds(250));
      if (!batch)
        break;

      for (const auto &event : batch->events)
        combined.events.push_back(event);
      combined.overflowed = combined.overflowed || batch->overflowed;

      if (has_path(combined, path))
        return combined;
    }

    assert(false && "timed out waiting for watched path");
    return combined;
  }

  static bool has_path(const watch::Batch &batch, const fs::path &path)
  {
    const fs::path normalized = path.lexically_normal();
    for (const auto &event : batch.events)
    {
      if (event.path.lexically_normal() == normalized)
        return true;
    }
    return false;
  }

  static void lifecycle_and_create_modify_remove()
  {
    TempDir tmp;
    watch::Options options;
    options.root = tmp.path;
    options.debounce = std::chrono::milliseconds(25);
    options.maxBatchWindow = std::chrono::milliseconds(100);

    watch::FileWatcher watcher(options);
    const watch::Result started = watcher.start();
    assert(started.ok);
    assert(watcher.running());
    assert(!watcher.backend().empty());

    const fs::path file = tmp.path / "src" / "main.cpp";
    write_file(file, "int main(){return 0;}\n");
    watch::Batch batch = wait_for_path(watcher, file);

    write_file(file, "int main(){return 1;}\n");
    batch = wait_for_path(watcher, file);

    fs::remove(file);
    batch = wait_for_path(watcher, file);

    watcher.stop();
    assert(!watcher.running());
  }

  static void recursive_new_directory_and_ignore_root()
  {
    TempDir tmp;
    const fs::path ignored = tmp.path / "build-ninja";

    watch::Options options;
    options.root = tmp.path;
    options.ignoredRoots.push_back(ignored);

    watch::FileWatcher watcher(options);
    assert(watcher.start().ok);

    const fs::path nested = tmp.path / "include" / "app" / "router.hpp";
    write_file(nested, "#pragma once\n");
    watch::Batch batch = wait_for_path(watcher, nested);

    write_file(ignored / "generated.o", "x");
    batch = watcher.wait_for_batch(std::chrono::milliseconds(200)).value_or(watch::Batch{});
    assert(batch.empty());

    watcher.stop();
  }

  static void rename_and_duplicate_coalescing()
  {
    TempDir tmp;
    watch::Options options;
    options.root = tmp.path;
    options.debounce = std::chrono::milliseconds(25);
    options.maxBatchWindow = std::chrono::milliseconds(100);

    watch::FileWatcher watcher(options);
    assert(watcher.start().ok);

    const fs::path a = tmp.path / "a.cpp";
    const fs::path b = tmp.path / "b.cpp";
    write_file(a, "int a;\n");
    (void)wait_non_empty(watcher);

    fs::rename(a, b);
    watch::Batch batch = wait_for_path(watcher, b);

    write_file(b, "int b;\n");
    write_file(b, "int c;\n");
    batch = wait_for_path(watcher, b);

    watcher.stop();
  }

  static void invalid_root()
  {
    watch::Options options;
    options.root = fs::temp_directory_path() / "vix-watch-missing-root";
    std::error_code ec;
    fs::remove_all(options.root, ec);

    watch::FileWatcher watcher(options);
    assert(!watcher.start().ok);
  }
}

int main()
{
  invalid_root();
  lifecycle_and_create_modify_remove();
  recursive_new_directory_and_ignore_root();
  rename_and_duplicate_coalescing();

  std::cout << "WatchTests passed\n";
  return 0;
}
