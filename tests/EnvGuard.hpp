#ifndef VIX_ENGINE_TESTS_ENV_GUARD_HPP
#define VIX_ENGINE_TESTS_ENV_GUARD_HPP

#include <cstdlib>
#include <optional>
#include <string>

#ifdef _WIN32
#include <stdlib.h>
#endif

namespace vix::engine::tests
{
  class EnvGuard
  {
  public:
    explicit EnvGuard(const char *name)
        : name_(name)
    {
      if (const char *value = std::getenv(name))
        oldValue_ = std::string(value);
    }

    EnvGuard(const EnvGuard &) = delete;
    EnvGuard &operator=(const EnvGuard &) = delete;

    ~EnvGuard()
    {
      if (oldValue_)
        set(*oldValue_);
      else
        unset();
    }

    void set(const std::string &value)
    {
#ifdef _WIN32
      _putenv_s(name_.c_str(), value.c_str());
#else
      ::setenv(name_.c_str(), value.c_str(), 1);
#endif
    }

    void unset()
    {
#ifdef _WIN32
      _putenv_s(name_.c_str(), "");
#else
      ::unsetenv(name_.c_str());
#endif
    }

  private:
    std::string name_;
    std::optional<std::string> oldValue_;
  };
} // namespace vix::engine::tests

#endif
