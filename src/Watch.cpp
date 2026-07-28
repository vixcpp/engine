/**
 *
 *  @file Watch.cpp
 *  @author Gaspard Kirira
 *
 *  Vix.cpp
 *
 */

#include <vix/engine/Watch.hpp>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <set>
#include <system_error>
#include <thread>
#include <unordered_map>

#if defined(__linux__)
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

namespace vix::engine::watch
{
  namespace
  {
    static fs::path normalized(const fs::path &p)
    {
      return p.lexically_normal();
    }

    static bool path_within(const fs::path &path, const fs::path &root)
    {
      const fs::path p = normalized(path);
      const fs::path r = normalized(root);

      auto pit = p.begin();
      auto rit = r.begin();

      for (; rit != r.end(); ++rit, ++pit)
      {
        if (pit == p.end() || *pit != *rit)
          return false;
      }

      return true;
    }

    static bool ignored_path(const Options &options, const fs::path &path)
    {
      const fs::path p = normalized(path);

      for (const fs::path &root : options.ignoredRoots)
      {
        if (!root.empty() && path_within(p, root))
          return true;
      }

      const std::string name = p.filename().string();
      if (name == ".git" ||
          name == ".hg" ||
          name == ".svn" ||
          name == ".vix" ||
          name == "node_modules" ||
          name == ".cache" ||
          name == ".idea" ||
          name == ".vscode" ||
          name == ".vix-build-state" ||
          name == ".vix-build-graph" ||
          name == ".vix-config.sig" ||
          name == "compile_commands.json" ||
          name == "build.ninja" ||
          name == "CMakeCache.txt" ||
          name == "CMakeFiles" ||
          name == "configure.log" ||
          name == "build.log")
      {
        return true;
      }

      return false;
    }

    static void sort_events(std::vector<Event> &events)
    {
      std::sort(
          events.begin(),
          events.end(),
          [](const Event &a, const Event &b)
          {
            const std::string ap = a.path.generic_string();
            const std::string bp = b.path.generic_string();
            if (ap != bp)
              return ap < bp;
            return static_cast<int>(a.kind) < static_cast<int>(b.kind);
          });
    }

    static Batch coalesce_events(
        const std::vector<Event> &input,
        bool overflowed)
    {
      Batch batch;
      batch.overflowed = overflowed;

      std::map<std::string, Event> byPath;

      for (Event e : input)
      {
        e.path = normalized(e.path);
        e.oldPath = normalized(e.oldPath);

        if (e.path.empty())
          continue;

        const std::string key = e.path.generic_string();
        auto it = byPath.find(key);

        if (it == byPath.end())
        {
          byPath.emplace(key, std::move(e));
          continue;
        }

        Event &old = it->second;

        if (old.kind == EventKind::Added && e.kind == EventKind::Modified)
          continue;

        if (e.kind == EventKind::Removed)
        {
          old.kind = EventKind::Removed;
          old.directory = old.directory || e.directory;
          continue;
        }

        if (old.kind == EventKind::Modified && e.kind == EventKind::Modified)
          continue;

        if (e.kind == EventKind::Renamed)
        {
          old = e;
          continue;
        }

        if (old.kind == EventKind::Removed && e.kind == EventKind::Added)
        {
          old.kind = EventKind::Modified;
          old.directory = e.directory;
          continue;
        }
      }

      for (auto &kv : byPath)
        batch.events.push_back(std::move(kv.second));

      sort_events(batch.events);
      return batch;
    }

    struct FileSnapshot
    {
      std::uintmax_t size{0};
      fs::file_time_type mtime{};
      bool directory{false};
    };

    class PollingWatcher
    {
    public:
      explicit PollingWatcher(Options options) : options_(std::move(options)) {}

      Result start()
      {
        std::error_code ec;
        if (options_.root.empty() || !fs::exists(options_.root, ec) || ec)
          return {false, "watch root does not exist", "polling"};

        options_.root = fs::absolute(options_.root, ec).lexically_normal();
        snapshot_ = scan();
        running_ = true;
        return {true, {}, "polling"};
      }

      void stop() { running_ = false; }
      bool running() const { return running_; }

      std::optional<Batch> wait_for_batch(std::chrono::milliseconds timeout)
      {
        if (!running_)
          return std::nullopt;

        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (running_ && std::chrono::steady_clock::now() < deadline)
        {
          std::this_thread::sleep_for(
              std::min(options_.pollingInterval, std::chrono::milliseconds(25)));

          auto next = scan();
          std::vector<Event> events = diff(snapshot_, next);

          if (!events.empty())
          {
            snapshot_ = std::move(next);
            if (options_.debounce.count() > 0)
              std::this_thread::sleep_for(options_.debounce);
            return coalesce_events(events, false);
          }
        }

        return Batch{};
      }

    private:
      std::map<std::string, FileSnapshot> scan() const
      {
        std::map<std::string, FileSnapshot> out;
        std::error_code ec;

        fs::recursive_directory_iterator it(
            options_.root,
            fs::directory_options::skip_permission_denied,
            ec);
        const fs::recursive_directory_iterator end;

        while (!ec && it != end)
        {
          const fs::path current = it->path().lexically_normal();

          if (ignored_path(options_, current))
          {
            if (it->is_directory(ec))
              it.disable_recursion_pending();
            ++it;
            continue;
          }

          FileSnapshot snap;
          snap.directory = it->is_directory(ec);

          if (!snap.directory && !it->is_regular_file(ec))
          {
            ++it;
            continue;
          }

          if (!snap.directory)
            snap.size = fs::file_size(current, ec);

          snap.mtime = fs::last_write_time(current, ec);
          out[current.generic_string()] = snap;

          ++it;
        }

        return out;
      }

      static std::vector<Event> diff(
          const std::map<std::string, FileSnapshot> &oldSnap,
          const std::map<std::string, FileSnapshot> &newSnap)
      {
        std::vector<Event> events;

        for (const auto &kv : newSnap)
        {
          const auto oldIt = oldSnap.find(kv.first);
          if (oldIt == oldSnap.end())
          {
            events.push_back({EventKind::Added, kv.first, {}, kv.second.directory});
            continue;
          }

          if (oldIt->second.size != kv.second.size ||
              oldIt->second.mtime != kv.second.mtime)
          {
            events.push_back({EventKind::Modified, kv.first, {}, kv.second.directory});
          }
        }

        for (const auto &kv : oldSnap)
        {
          if (newSnap.find(kv.first) == newSnap.end())
            events.push_back({EventKind::Removed, kv.first, {}, kv.second.directory});
        }

        return events;
      }

      Options options_;
      bool running_{false};
      std::map<std::string, FileSnapshot> snapshot_;
    };

#if defined(__linux__)
    class InotifyWatcher
    {
    public:
      explicit InotifyWatcher(Options options) : options_(std::move(options)) {}

      ~InotifyWatcher() { stop(); }

      Result start()
      {
        std::error_code ec;
        if (options_.root.empty() || !fs::exists(options_.root, ec) || ec)
          return {false, "watch root does not exist", "inotify"};

        options_.root = fs::absolute(options_.root, ec).lexically_normal();

        fd_ = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (fd_ < 0)
          return {false, std::string("inotify_init1 failed: ") + std::strerror(errno), "inotify"};

        add_directory_recursive(options_.root);

        if (wdToPath_.empty())
        {
          stop();
          return {false, "no watchable directories under root", "inotify"};
        }

        running_ = true;
        return {true, {}, "inotify"};
      }

      void stop()
      {
        running_ = false;
        wdToPath_.clear();
        pathToWd_.clear();

        if (fd_ >= 0)
        {
          ::close(fd_);
          fd_ = -1;
        }
      }

      bool running() const { return running_; }

      std::optional<Batch> wait_for_batch(std::chrono::milliseconds timeout)
      {
        if (!running_ || fd_ < 0)
          return std::nullopt;

        std::vector<Event> events;
        bool overflowed = false;
        const auto waitStart = std::chrono::steady_clock::now();
        auto lastEvent = waitStart;
        bool sawEvent = false;

        while (running_)
        {
          std::chrono::milliseconds wait = timeout;

          if (sawEvent)
          {
            const auto now = std::chrono::steady_clock::now();
            const auto sinceLast =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEvent);
            const auto sinceStart =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - waitStart);

            if (sinceLast >= options_.debounce ||
                sinceStart >= options_.maxBatchWindow)
            {
              return coalesce_events(events, overflowed);
            }

            wait = std::min(
                options_.debounce - sinceLast,
                options_.maxBatchWindow - sinceStart);
          }

          struct pollfd pfd;
          pfd.fd = fd_;
          pfd.events = POLLIN;
          pfd.revents = 0;

          const int pollMs =
              static_cast<int>(std::max<long long>(0, wait.count()));
          const int pr = ::poll(&pfd, 1, pollMs);

          if (pr < 0)
          {
            if (errno == EINTR)
              continue;
            Event event;
            event.kind = EventKind::Overflow;
            event.path = options_.root;
            return Batch{{event}, true};
          }

          if (pr == 0)
          {
            if (sawEvent)
              return coalesce_events(events, overflowed);
            return Batch{};
          }

          char buffer[64 * 1024];
          while (true)
          {
            const ssize_t n = ::read(fd_, buffer, sizeof(buffer));
            if (n < 0)
            {
              if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
              if (errno == EINTR)
                continue;
              overflowed = true;
              break;
            }

            if (n == 0)
              break;

            std::size_t offset = 0;
            while (offset < static_cast<std::size_t>(n))
            {
              const auto *ev =
                  reinterpret_cast<const struct inotify_event *>(buffer + offset);
              handle_event(*ev, events, overflowed);
              offset += sizeof(struct inotify_event) + ev->len;
            }
          }

          if (!events.empty() || overflowed)
          {
            sawEvent = true;
            lastEvent = std::chrono::steady_clock::now();
          }
        }

        return std::nullopt;
      }

    private:
      void add_directory_recursive(const fs::path &dir)
      {
        add_directory(dir);

        if (!options_.recursive)
          return;

        std::error_code ec;
        fs::recursive_directory_iterator it(
            dir,
            fs::directory_options::skip_permission_denied,
            ec);
        const fs::recursive_directory_iterator end;

        while (!ec && it != end)
        {
          const fs::path current = it->path().lexically_normal();

          if (!it->is_directory(ec))
          {
            ++it;
            continue;
          }

          if (ignored_path(options_, current))
          {
            it.disable_recursion_pending();
            ++it;
            continue;
          }

          add_directory(current);
          ++it;
        }
      }

      void add_directory(const fs::path &dir)
      {
        const fs::path p = dir.lexically_normal();
        if (ignored_path(options_, p))
          return;

        const std::string key = p.generic_string();
        if (pathToWd_.find(key) != pathToWd_.end())
          return;

        constexpr std::uint32_t mask =
            IN_CREATE |
            IN_MODIFY |
            IN_CLOSE_WRITE |
            IN_DELETE |
            IN_MOVED_FROM |
            IN_MOVED_TO |
            IN_ATTRIB |
            IN_DELETE_SELF |
            IN_MOVE_SELF |
            IN_Q_OVERFLOW;

        const int wd = ::inotify_add_watch(fd_, key.c_str(), mask);
        if (wd < 0)
          return;

        wdToPath_[wd] = p;
        pathToWd_[key] = wd;
      }

      void handle_event(
          const struct inotify_event &ev,
          std::vector<Event> &events,
          bool &overflowed)
      {
        if (ev.mask & IN_Q_OVERFLOW)
        {
          overflowed = true;
          Event event;
          event.kind = EventKind::Overflow;
          event.path = options_.root;
          events.push_back(event);
          return;
        }

        const auto dirIt = wdToPath_.find(ev.wd);
        if (dirIt == wdToPath_.end())
          return;

        fs::path path = dirIt->second;
        if (ev.len > 0 && ev.name[0] != '\0')
          path /= ev.name;

        path = path.lexically_normal();
        const bool isDir = (ev.mask & IN_ISDIR) != 0;

        if (ignored_path(options_, path))
          return;

        if (isDir && (ev.mask & (IN_CREATE | IN_MOVED_TO)))
        {
          add_directory_recursive(path);
          emit_existing_children(path, events);
        }

        if (ev.mask & IN_MOVED_FROM)
        {
          if (ev.cookie != 0)
            pendingRenames_[ev.cookie] = path;
          else
            events.push_back({EventKind::Removed, path, {}, isDir});
          return;
        }

        if (ev.mask & IN_MOVED_TO)
        {
          fs::path oldPath;
          const auto pendingIt = pendingRenames_.find(ev.cookie);
          if (pendingIt != pendingRenames_.end())
          {
            oldPath = pendingIt->second;
            pendingRenames_.erase(pendingIt);
            events.push_back({EventKind::Renamed, path, oldPath, isDir});
          }
          else
          {
            events.push_back({EventKind::Added, path, {}, isDir});
          }
          return;
        }

        if (ev.mask & (IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF))
        {
          events.push_back({EventKind::Removed, path, {}, isDir});
          return;
        }

        if (ev.mask & IN_CREATE)
        {
          events.push_back({EventKind::Added, path, {}, isDir});
          return;
        }

        if (ev.mask & (IN_CLOSE_WRITE | IN_MODIFY | IN_ATTRIB))
        {
          events.push_back({EventKind::Modified, path, {}, isDir});
          return;
        }
      }

      void emit_existing_children(
          const fs::path &dir,
          std::vector<Event> &events)
      {
        std::error_code ec;
        fs::recursive_directory_iterator it(
            dir,
            fs::directory_options::skip_permission_denied,
            ec);
        const fs::recursive_directory_iterator end;

        while (!ec && it != end)
        {
          const fs::path current = it->path().lexically_normal();

          if (ignored_path(options_, current))
          {
            if (it->is_directory(ec))
              it.disable_recursion_pending();
            ++it;
            continue;
          }

          const bool isDir = it->is_directory(ec);
          if (isDir)
          {
            ++it;
            continue;
          }

          if (it->is_regular_file(ec))
            events.push_back({EventKind::Added, current, {}, false});

          ++it;
        }
      }

      Options options_;
      int fd_{-1};
      bool running_{false};
      std::unordered_map<int, fs::path> wdToPath_;
      std::unordered_map<std::string, int> pathToWd_;
      std::unordered_map<std::uint32_t, fs::path> pendingRenames_;
    };
#endif
  } // namespace

  const char *to_string(EventKind kind)
  {
    switch (kind)
    {
    case EventKind::Added:
      return "added";
    case EventKind::Modified:
      return "modified";
    case EventKind::Removed:
      return "removed";
    case EventKind::Renamed:
      return "renamed";
    case EventKind::Overflow:
      return "overflow";
    }

    return "modified";
  }

  bool Batch::empty() const
  {
    return events.empty() && !overflowed;
  }

  class FileWatcher::Impl
  {
  public:
    explicit Impl(Options options) : options_(std::move(options)) {}

    Result start()
    {
#if defined(__linux__)
      native_ = std::make_unique<InotifyWatcher>(options_);
      Result nativeResult = native_->start();
      if (nativeResult.ok)
      {
        backend_ = nativeResult.backend;
        return nativeResult;
      }
      native_.reset();
#endif

      polling_ = std::make_unique<PollingWatcher>(options_);
      Result result = polling_->start();
      backend_ = result.backend;
      return result;
    }

    std::optional<Batch> wait_for_batch(std::chrono::milliseconds timeout)
    {
#if defined(__linux__)
      if (native_)
        return native_->wait_for_batch(timeout);
#endif
      if (polling_)
        return polling_->wait_for_batch(timeout);
      return std::nullopt;
    }

    void stop()
    {
#if defined(__linux__)
      if (native_)
        native_->stop();
#endif
      if (polling_)
        polling_->stop();
    }

    bool running() const
    {
#if defined(__linux__)
      if (native_)
        return native_->running();
#endif
      return polling_ && polling_->running();
    }

    const std::string &backend() const { return backend_; }

  private:
    Options options_;
    std::string backend_;
#if defined(__linux__)
    std::unique_ptr<InotifyWatcher> native_;
#endif
    std::unique_ptr<PollingWatcher> polling_;
  };

  FileWatcher::FileWatcher(Options options)
      : options_(std::move(options)),
        impl_(std::make_unique<Impl>(options_))
  {
  }

  FileWatcher::~FileWatcher()
  {
    stop();
  }

  Result FileWatcher::start()
  {
    Result result = impl_->start();
    backend_ = impl_->backend();
    return result;
  }

  std::optional<Batch> FileWatcher::wait_for_batch(
      std::chrono::milliseconds timeout)
  {
    return impl_->wait_for_batch(timeout);
  }

  void FileWatcher::stop()
  {
    if (impl_)
      impl_->stop();
  }

  bool FileWatcher::running() const
  {
    return impl_ && impl_->running();
  }

  const Options &FileWatcher::options() const
  {
    return options_;
  }

  const std::string &FileWatcher::backend() const
  {
    return backend_;
  }

} // namespace vix::engine::watch
