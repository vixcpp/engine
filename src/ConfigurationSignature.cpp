/**
 * @file ConfigurationSignature.cpp
 *
 * Configuration signature generation and persistence.
 */
#include <vix/engine/ConfigurationSignature.hpp>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

namespace vix::engine
{
  namespace
  {
    std::string trim(std::string value)
    {
      auto notSpace = [](unsigned char ch)
      {
        return !std::isspace(ch);
      };

      while (!value.empty() && !notSpace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());

      while (!value.empty() && !notSpace(static_cast<unsigned char>(value.back())))
        value.pop_back();

      return value;
    }

    std::string signature_join(
        const std::vector<std::pair<std::string, std::string>> &variables)
    {
      std::ostringstream out;
      for (const auto &variable : variables)
        out << variable.first << "=" << variable.second << "\n";
      return out.str();
    }
  } // namespace

  std::string make_configuration_signature(
      const ExecutionPlan &plan,
      const ConfigurationSignatureOptions &options)
  {
    std::ostringstream oss;

    oss << "preset=" << plan.preset.name << "\n";
    oss << "generator=" << plan.preset.generator << "\n";
    oss << "buildType=" << plan.preset.buildType << "\n";
    oss << "static=" << (options.linkStatic ? "1" : "0") << "\n";
    oss << "targetTriple=" << options.targetTriple << "\n";
    oss << "sysroot=" << options.sysroot << "\n";
    oss << "fast=" << (options.fast ? "1" : "0") << "\n";
    oss << "useCache=" << (options.useCache ? "1" : "0") << "\n";
    oss << "warningCheck=" << (options.warningCheck ? "1" : "0") << "\n";
    oss << "linker=" << static_cast<int>(options.linker) << "\n";
    oss << "launcher=" << static_cast<int>(options.launcher) << "\n";
    oss << "verbose=" << (options.verbose ? "1" : "0") << "\n";
    oss << "cmakeVerbose=" << (options.cmakeVerbose ? "1" : "0") << "\n";

    if (plan.launcher)
      oss << "launcherTool=" << *plan.launcher << "\n";

    if (plan.fastLinkerFlag)
      oss << "linkerFlag=" << *plan.fastLinkerFlag << "\n";

    oss << "projectFingerprint=" << plan.projectFingerprint << "\n";

    oss << "vars:\n";
    oss << signature_join(plan.cmakeVars);

    oss << "rawCMakeArgs:\n";
    for (const auto &arg : options.rawCMakeArgs)
      oss << arg << "\n";

    if (!options.targetTriple.empty())
    {
      oss << "toolchain:\n";
      oss << options.toolchainContent;
      if (!options.toolchainContent.empty() &&
          options.toolchainContent.back() != '\n')
      {
        oss << "\n";
      }
    }

    return trim(oss.str()) + "\n";
  }

  std::optional<std::string> read_configuration_signature(
      const std::filesystem::path &signatureFile)
  {
    std::ifstream input(signatureFile, std::ios::binary);
    if (!input)
      return std::nullopt;

    std::ostringstream buffer;
    buffer << input.rdbuf();

    if (!input.good() && !input.eof())
      return std::nullopt;

    return buffer.str();
  }

  bool write_configuration_signature(
      const std::filesystem::path &signatureFile,
      std::string_view signature)
  {
    std::error_code ec;
    if (!signatureFile.parent_path().empty())
    {
      std::filesystem::create_directories(signatureFile.parent_path(), ec);
      if (ec)
        return false;
    }

    const std::filesystem::path tmp = signatureFile.string() + ".tmp";

    {
      std::ofstream output(tmp, std::ios::binary);
      if (!output)
        return false;

      output.write(
          signature.data(),
          static_cast<std::streamsize>(signature.size()));
      output.flush();
      if (!output)
        return false;
    }

    std::filesystem::rename(tmp, signatureFile, ec);
    if (ec)
    {
      std::filesystem::remove(tmp, ec);
      return false;
    }

    return true;
  }

  bool configuration_signature_matches(
      const std::filesystem::path &signatureFile,
      std::string_view expectedSignature)
  {
    const std::optional<std::string> existing =
        read_configuration_signature(signatureFile);

    return existing.has_value() &&
           *existing == expectedSignature;
  }
} // namespace vix::engine
