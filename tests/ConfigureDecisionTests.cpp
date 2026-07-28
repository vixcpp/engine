#include <vix/engine/ConfigureDecision.hpp>
#include <vix/engine/ConfigurationSignature.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>

namespace
{
  namespace fs = std::filesystem;
  using namespace vix::engine;

  fs::path temp_root()
  {
    fs::path root = fs::temp_directory_path() / "vix-engine-configure-decision";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    assert(!ec);
    return root;
  }

  void write_file(const fs::path &path, const std::string &content)
  {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
  }
}

int main()
{
  const fs::path root = temp_root();
  const fs::path build = root / "build-ninja";
  const fs::path sig = build / ".vix-config.sig";

  assert(to_string(ConfigureReason::UpToDate) == std::string("up-to-date"));
  assert(to_string(ConfigureReason::CacheDisabled) == std::string("cache-disabled"));
  assert(to_string(ConfigureReason::CleanRequested) == std::string("clean-requested"));
  assert(to_string(ConfigureReason::MissingCMakeCache) == std::string("missing-cmake-cache"));
  assert(to_string(ConfigureReason::MissingSignature) == std::string("missing-signature"));
  assert(to_string(ConfigureReason::SignatureMismatch) == std::string("signature-mismatch"));
  assert(cmake_cache_path(build) == build / "CMakeCache.txt");

  ConfigureDecisionOptions options;
  options.buildDir = build;
  options.signatureFile = sig;
  options.expectedSignature = "expected\n";

  auto decision = evaluate_configuration(options);
  assert(decision.needs_configure());
  assert(decision.reason == ConfigureReason::MissingCMakeCache);
  assert(decision.cmakeCacheFile == build / "CMakeCache.txt");
  assert(decision.signatureFile == sig);
  assert(decision.expectedSignature == "expected\n");
  assert(!decision.existingSignature);
  assert(needs_configure(options));

  options.useCache = false;
  options.clean = true;
  decision = evaluate_configuration(options);
  assert(decision.reason == ConfigureReason::CacheDisabled);

  options.useCache = true;
  options.clean = true;
  decision = evaluate_configuration(options);
  assert(decision.reason == ConfigureReason::CleanRequested);

  write_file(build / "CMakeCache.txt", "cache");
  options.clean = true;
  decision = evaluate_configuration(options);
  assert(decision.reason == ConfigureReason::CleanRequested);

  options.clean = false;
  decision = evaluate_configuration(options);
  assert(decision.reason == ConfigureReason::MissingSignature);

  assert(write_configuration_signature(sig, "old\n"));
  decision = evaluate_configuration(options);
  assert(decision.reason == ConfigureReason::SignatureMismatch);
  assert(decision.existingSignature == "old\n");

  assert(write_configuration_signature(sig, "expected\n"));
  decision = evaluate_configuration(options);
  assert(!decision.needs_configure());
  assert(decision.reason == ConfigureReason::UpToDate);
  assert(decision.existingSignature == "expected\n");
  assert(!needs_configure(options));

  fs::remove(build / "CMakeCache.txt");
  fs::remove(sig);
  decision = evaluate_configuration(options);
  assert(decision.reason == ConfigureReason::MissingCMakeCache);

  return 0;
}
