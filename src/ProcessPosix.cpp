#include <vix/engine/Process.hpp>

#ifndef _WIN32

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char **environ;

namespace vix::engine::process
{
  namespace
  {
    static void close_fd(int fd) noexcept
    {
      if (fd >= 0)
        ::close(fd);
    }

    static bool set_close_on_exec(int fd) noexcept
    {
      const int flags = ::fcntl(fd, F_GETFD);
      if (flags < 0)
        return false;
      return ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
    }

    static std::string errno_message(const char *prefix, int code)
    {
      std::ostringstream out;
      out << prefix << ": " << std::strerror(code);
      return out.str();
    }

    static std::vector<std::string> build_environment_strings(const Command &command)
    {
      std::map<std::string, std::string> env;

      if (command.inheritEnvironment)
      {
        for (char **it = environ; it && *it; ++it)
        {
          std::string entry(*it);
          const auto eq = entry.find('=');
          if (eq == std::string::npos)
            continue;

          env[entry.substr(0, eq)] = entry.substr(eq + 1);
        }
      }

      for (const auto &kv : command.environment)
        env[kv.first] = kv.second;

      std::vector<std::string> strings;
      strings.reserve(env.size());
      for (const auto &kv : env)
        strings.push_back(kv.first + "=" + kv.second);

      return strings;
    }

    static const char *find_environment_value(
        std::vector<std::string> &envStrings,
        const char *name)
    {
      const std::string prefix = std::string(name) + "=";

      for (auto &entry : envStrings)
      {
        if (entry.rfind(prefix, 0) == 0)
          return entry.c_str() + prefix.size();
      }

      return nullptr;
    }

    static void exec_with_environment(
        const std::vector<std::string> &args,
        std::vector<char *> &argv,
        std::vector<std::string> &envStrings,
        std::vector<char *> &envp)
    {
      if (args.front().find('/') != std::string::npos)
      {
        ::execve(args.front().c_str(), argv.data(), envp.data());
        return;
      }

      const char *pathValue = find_environment_value(envStrings, "PATH");
      if (!pathValue || !*pathValue)
      {
        ::execve(args.front().c_str(), argv.data(), envp.data());
        return;
      }

      std::string path(pathValue);
      std::size_t begin = 0;

      while (begin <= path.size())
      {
        const std::size_t end = path.find(':', begin);
        const std::string dir =
            path.substr(begin, end == std::string::npos ? std::string::npos : end - begin);

        const std::string candidate =
            (dir.empty() ? "." : dir) + "/" + args.front();

        ::execve(candidate.c_str(), argv.data(), envp.data());

        if (end == std::string::npos)
          break;

        begin = end + 1;
      }
    }

    static bool read_available(int fd, std::string &out, bool &open)
    {
      char buffer[8192];

      while (true)
      {
        const ssize_t n = ::read(fd, buffer, sizeof(buffer));
        if (n > 0)
        {
          out.append(buffer, static_cast<std::size_t>(n));
          continue;
        }

        if (n == 0)
        {
          open = false;
          close_fd(fd);
          return true;
        }

        if (errno == EINTR)
          continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
          return true;

        open = false;
        close_fd(fd);
        return false;
      }
    }
  } // namespace

  int normalize_exit_code(int raw) noexcept
  {
    if (raw == -1)
      return 127;

    if (WIFEXITED(raw))
      return WEXITSTATUS(raw);

    if (WIFSIGNALED(raw))
      return 128 + WTERMSIG(raw);

    return raw;
  }

  Result execute(const Command &command)
  {
    Result result;
    result.displayCommand = display_command(command);

    if (!command.valid())
    {
      result.exitCode = 127;
      result.errorMessage = "Empty process command.";
      return result;
    }

    int stdoutPipe[2] = {-1, -1};
    int stderrPipe[2] = {-1, -1};
    int errorPipe[2] = {-1, -1};

    if (::pipe(stdoutPipe) != 0)
    {
      result.exitCode = 127;
      result.errorMessage = errno_message("pipe failed", errno);
      return result;
    }

    if (!command.mergeStdErr && ::pipe(stderrPipe) != 0)
    {
      result.exitCode = 127;
      result.errorMessage = errno_message("pipe failed", errno);
      close_fd(stdoutPipe[0]);
      close_fd(stdoutPipe[1]);
      return result;
    }

    if (::pipe(errorPipe) != 0)
    {
      result.exitCode = 127;
      result.errorMessage = errno_message("pipe failed", errno);
      close_fd(stdoutPipe[0]);
      close_fd(stdoutPipe[1]);
      close_fd(stderrPipe[0]);
      close_fd(stderrPipe[1]);
      return result;
    }

    (void)set_close_on_exec(errorPipe[1]);

    const pid_t pid = ::fork();
    if (pid < 0)
    {
      result.exitCode = 127;
      result.errorMessage = errno_message("fork failed", errno);
      close_fd(stdoutPipe[0]);
      close_fd(stdoutPipe[1]);
      close_fd(stderrPipe[0]);
      close_fd(stderrPipe[1]);
      close_fd(errorPipe[0]);
      close_fd(errorPipe[1]);
      return result;
    }

    if (pid == 0)
    {
      close_fd(stdoutPipe[0]);
      close_fd(errorPipe[0]);

      if (::dup2(stdoutPipe[1], STDOUT_FILENO) < 0)
      {
        const int code = errno;
        (void)::write(errorPipe[1], &code, sizeof(code));
        _exit(127);
      }

      if (command.mergeStdErr)
      {
        if (::dup2(stdoutPipe[1], STDERR_FILENO) < 0)
        {
          const int code = errno;
          (void)::write(errorPipe[1], &code, sizeof(code));
          _exit(127);
        }
      }
      else
      {
        close_fd(stderrPipe[0]);
        if (::dup2(stderrPipe[1], STDERR_FILENO) < 0)
        {
          const int code = errno;
          (void)::write(errorPipe[1], &code, sizeof(code));
          _exit(127);
        }
      }

      close_fd(stdoutPipe[1]);
      close_fd(stderrPipe[1]);

      if (!command.workingDirectory.empty() &&
          ::chdir(command.workingDirectory.c_str()) != 0)
      {
        const int code = errno;
        (void)::write(errorPipe[1], &code, sizeof(code));
        _exit(127);
      }

      std::vector<char *> argv;
      argv.reserve(command.argv.size() + 1);
      for (const auto &arg : command.argv)
        argv.push_back(const_cast<char *>(arg.c_str()));
      argv.push_back(nullptr);

      if (command.inheritEnvironment && command.environment.empty())
      {
        ::execvp(argv[0], argv.data());
      }
      else
      {
        std::vector<std::string> envStrings = build_environment_strings(command);
        std::vector<char *> envp;
        envp.reserve(envStrings.size() + 1);
        for (auto &entry : envStrings)
          envp.push_back(entry.data());
        envp.push_back(nullptr);

        exec_with_environment(command.argv, argv, envStrings, envp);
      }

      const int code = errno;
      (void)::write(errorPipe[1], &code, sizeof(code));
      _exit(127);
    }

    result.started = true;

    close_fd(stdoutPipe[1]);
    close_fd(stderrPipe[1]);
    close_fd(errorPipe[1]);

    (void)::fcntl(stdoutPipe[0], F_SETFL, ::fcntl(stdoutPipe[0], F_GETFL, 0) | O_NONBLOCK);
    if (!command.mergeStdErr)
      (void)::fcntl(stderrPipe[0], F_SETFL, ::fcntl(stderrPipe[0], F_GETFL, 0) | O_NONBLOCK);
    (void)::fcntl(errorPipe[0], F_SETFL, ::fcntl(errorPipe[0], F_GETFL, 0) | O_NONBLOCK);

    bool stdoutOpen = true;
    bool stderrOpen = !command.mergeStdErr;
    bool errorOpen = true;
    std::string startupErrorBytes;
    int status = 0;
    bool waited = false;

    while (stdoutOpen || stderrOpen || errorOpen || !waited)
    {
      fd_set readSet;
      FD_ZERO(&readSet);
      int maxFd = -1;

      if (stdoutOpen)
      {
        FD_SET(stdoutPipe[0], &readSet);
        maxFd = std::max(maxFd, stdoutPipe[0]);
      }

      if (stderrOpen)
      {
        FD_SET(stderrPipe[0], &readSet);
        maxFd = std::max(maxFd, stderrPipe[0]);
      }

      if (errorOpen)
      {
        FD_SET(errorPipe[0], &readSet);
        maxFd = std::max(maxFd, errorPipe[0]);
      }

      if (maxFd >= 0)
      {
        const int selected = ::select(maxFd + 1, &readSet, nullptr, nullptr, nullptr);
        if (selected < 0 && errno != EINTR)
          break;
      }

      if (stdoutOpen && FD_ISSET(stdoutPipe[0], &readSet))
        (void)read_available(stdoutPipe[0], result.output, stdoutOpen);

      if (stderrOpen && FD_ISSET(stderrPipe[0], &readSet))
        (void)read_available(stderrPipe[0], result.output, stderrOpen);

      if (errorOpen && FD_ISSET(errorPipe[0], &readSet))
        (void)read_available(errorPipe[0], startupErrorBytes, errorOpen);

      if (!waited)
      {
        const pid_t waitedPid = ::waitpid(pid, &status, WNOHANG);
        if (waitedPid == pid)
          waited = true;
        else if (waitedPid < 0 && errno != EINTR)
        {
          waited = true;
          status = -1;
        }
      }

      if (waited && !stdoutOpen && !stderrOpen && !errorOpen)
        break;
    }

    while (!waited)
    {
      const pid_t waitedPid = ::waitpid(pid, &status, 0);
      if (waitedPid == pid)
        waited = true;
      else if (waitedPid < 0 && errno != EINTR)
      {
        status = -1;
        waited = true;
      }
    }

    result.exitCode = normalize_exit_code(status);
    result.producedOutput = !result.output.empty();

    if (!startupErrorBytes.empty())
    {
      int code = 0;
      std::memcpy(&code, startupErrorBytes.data(), std::min(sizeof(code), startupErrorBytes.size()));
      result.errorMessage = errno_message("process startup failed", code);
      result.exitCode = 127;
    }

    return result;
  }
} // namespace vix::engine::process

#endif
