/**
 * @file SanitizerMode.hpp
 *
 * Canonical sanitizer build modes.
 */
#ifndef VIX_ENGINE_SANITIZER_MODE_HPP
#define VIX_ENGINE_SANITIZER_MODE_HPP

#include <string_view>
#include <optional>

namespace vix::engine
{
  /**
   * @brief Sanitizer instrumentation selected for a build.
   *
   * ThreadSanitizer is intentionally represented as a separate mode
   * because it cannot be combined with AddressSanitizer.
   */
  enum class SanitizerMode
  {
    None,
    Address,
    Undefined,
    AddressUndefined,
    Thread
  };

  /**
   * @brief Returns the canonical configuration name of a sanitizer mode.
   *
   * These values are suitable for configuration signatures, cache metadata,
   * build state files, and CMake variables.
   */
  [[nodiscard]] constexpr std::string_view
  sanitizer_mode_name(SanitizerMode mode) noexcept
  {
    switch (mode)
    {
    case SanitizerMode::None:
      return "none";

    case SanitizerMode::Address:
      return "address";

    case SanitizerMode::Undefined:
      return "undefined";

    case SanitizerMode::AddressUndefined:
      return "address-undefined";

    case SanitizerMode::Thread:
      return "thread";
    }

    return "none";
  }

  /**
   * @brief Returns the build-directory suffix for a sanitizer mode.
   *
   * Examples:
   * - build-ninja
   * - build-ninja-asan
   * - build-ninja-ubsan
   * - build-ninja-san
   * - build-ninja-tsan
   */
  [[nodiscard]] constexpr std::string_view
  sanitizer_build_suffix(SanitizerMode mode) noexcept
  {
    switch (mode)
    {
    case SanitizerMode::None:
      return "";

    case SanitizerMode::Address:
      return "asan";

    case SanitizerMode::Undefined:
      return "ubsan";

    case SanitizerMode::AddressUndefined:
      return "san";

    case SanitizerMode::Thread:
      return "tsan";
    }

    return "";
  }

  /**
   * @brief Returns true when sanitizer instrumentation is enabled.
   */
  [[nodiscard]] constexpr bool
  sanitizer_enabled(SanitizerMode mode) noexcept
  {
    return mode != SanitizerMode::None;
  }

  /**
   * @brief Parses a sanitizer mode name.
   *
   * Accepted public names:
   * - address
   * - undefined
   * - address,undefined
   * - thread
   *
   * Common sanitizer names are accepted as aliases:
   * - asan
   * - ubsan
   * - asan,ubsan
   * - tsan
   */
  [[nodiscard]] constexpr std::optional<SanitizerMode>
  parse_sanitizer_mode(std::string_view value) noexcept
  {
    if (value == "address" ||
        value == "asan")
    {
      return SanitizerMode::Address;
    }

    if (value == "undefined" ||
        value == "ubsan")
    {
      return SanitizerMode::Undefined;
    }

    if (value == "address,undefined" ||
        value == "undefined,address" ||
        value == "address-undefined" ||
        value == "asan,ubsan" ||
        value == "ubsan,asan" ||
        value == "asan-ubsan")
    {
      return SanitizerMode::AddressUndefined;
    }

    if (value == "thread" ||
        value == "tsan")
    {
      return SanitizerMode::Thread;
    }

    return std::nullopt;
  }

} // namespace vix::engine

#endif
