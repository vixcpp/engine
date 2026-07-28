/**
 * @file ConfigurationSignature.hpp
 *
 * Configuration signature generation and persistence.
 */
#ifndef VIX_ENGINE_CONFIGURATION_SIGNATURE_HPP
#define VIX_ENGINE_CONFIGURATION_SIGNATURE_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <vix/engine/BuildTools.hpp>
#include <vix/engine/ExecutionPlan.hpp>

namespace vix::engine
{
  struct ConfigurationSignatureOptions
  {
    bool linkStatic{false};

    std::string targetTriple;
    std::string sysroot;

    bool fast{false};
    bool useCache{true};
    bool warningCheck{false};

    LinkerMode linker{LinkerMode::Auto};
    LauncherMode launcher{LauncherMode::Auto};

    bool verbose{false};
    bool cmakeVerbose{false};

    std::vector<std::string> rawCMakeArgs;
    std::string toolchainContent;
  };

  std::string make_configuration_signature(
      const ExecutionPlan &plan,
      const ConfigurationSignatureOptions &options);

  std::optional<std::string> read_configuration_signature(
      const std::filesystem::path &signatureFile);

  bool write_configuration_signature(
      const std::filesystem::path &signatureFile,
      std::string_view signature);

  bool configuration_signature_matches(
      const std::filesystem::path &signatureFile,
      std::string_view expectedSignature);
} // namespace vix::engine

#endif
