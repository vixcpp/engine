/**
 * @file ConfigureDecision.cpp
 *
 * Structured CMake configure decision model.
 */
#include <vix/engine/ConfigureDecision.hpp>

#include <system_error>

#include <vix/engine/ConfigurationSignature.hpp>

namespace vix::engine
{
  const char *to_string(ConfigureReason reason)
  {
    switch (reason)
    {
    case ConfigureReason::UpToDate:
      return "up-to-date";
    case ConfigureReason::CacheDisabled:
      return "cache-disabled";
    case ConfigureReason::CleanRequested:
      return "clean-requested";
    case ConfigureReason::MissingCMakeCache:
      return "missing-cmake-cache";
    case ConfigureReason::MissingSignature:
      return "missing-signature";
    case ConfigureReason::SignatureMismatch:
      return "signature-mismatch";
    }

    return "missing-cmake-cache";
  }

  std::filesystem::path cmake_cache_path(
      const std::filesystem::path &buildDir)
  {
    return buildDir / "CMakeCache.txt";
  }

  bool has_cmake_cache(
      const std::filesystem::path &buildDir)
  {
    std::error_code ec;
    const std::filesystem::path cache = cmake_cache_path(buildDir);
    return std::filesystem::exists(cache, ec) && !ec;
  }

  bool ConfigureDecision::needs_configure() const
  {
    return configure;
  }

  ConfigureDecision evaluate_configuration(
      const ConfigureDecisionOptions &options)
  {
    ConfigureDecision decision;
    decision.cmakeCacheFile = cmake_cache_path(options.buildDir);
    decision.signatureFile = options.signatureFile;
    decision.expectedSignature = options.expectedSignature;

    if (!options.useCache)
    {
      decision.configure = true;
      decision.reason = ConfigureReason::CacheDisabled;
      return decision;
    }

    if (options.clean)
    {
      decision.configure = true;
      decision.reason = ConfigureReason::CleanRequested;
      return decision;
    }

    if (!has_cmake_cache(options.buildDir))
    {
      decision.configure = true;
      decision.reason = ConfigureReason::MissingCMakeCache;
      return decision;
    }

    decision.existingSignature =
        read_configuration_signature(options.signatureFile);

    if (!decision.existingSignature)
    {
      decision.configure = true;
      decision.reason = ConfigureReason::MissingSignature;
      return decision;
    }

    if (*decision.existingSignature != options.expectedSignature)
    {
      decision.configure = true;
      decision.reason = ConfigureReason::SignatureMismatch;
      return decision;
    }

    decision.configure = false;
    decision.reason = ConfigureReason::UpToDate;
    return decision;
  }

  bool needs_configure(
      const ConfigureDecisionOptions &options)
  {
    return evaluate_configuration(options).needs_configure();
  }
} // namespace vix::engine
