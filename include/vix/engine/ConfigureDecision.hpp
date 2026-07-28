/**
 * @file ConfigureDecision.hpp
 *
 * Structured CMake configure decision model.
 */
#ifndef VIX_ENGINE_CONFIGURE_DECISION_HPP
#define VIX_ENGINE_CONFIGURE_DECISION_HPP

#include <filesystem>
#include <optional>
#include <string>

namespace vix::engine
{
  enum class ConfigureReason
  {
    UpToDate,
    CacheDisabled,
    CleanRequested,
    MissingCMakeCache,
    MissingSignature,
    SignatureMismatch
  };

  const char *to_string(ConfigureReason reason);

  std::filesystem::path cmake_cache_path(
      const std::filesystem::path &buildDir);

  bool has_cmake_cache(
      const std::filesystem::path &buildDir);

  struct ConfigureDecisionOptions
  {
    bool useCache{true};
    bool clean{false};

    std::filesystem::path buildDir;
    std::filesystem::path signatureFile;
    std::string expectedSignature;
  };

  struct ConfigureDecision
  {
    bool configure{true};
    ConfigureReason reason{ConfigureReason::MissingCMakeCache};

    std::filesystem::path cmakeCacheFile;
    std::filesystem::path signatureFile;

    std::optional<std::string> existingSignature;
    std::string expectedSignature;

    [[nodiscard]] bool needs_configure() const;
  };

  ConfigureDecision evaluate_configuration(
      const ConfigureDecisionOptions &options);

  bool needs_configure(
      const ConfigureDecisionOptions &options);
} // namespace vix::engine

#endif
