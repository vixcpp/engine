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
    fs::path root = fs::temp_directory_path() / "vix-engine-signature-persistence";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    assert(!ec);
    return root;
  }
}

int main()
{
  const fs::path root = temp_root();
  const fs::path sig = root / "nested" / ".vix-config.sig";

  assert(!read_configuration_signature(sig).has_value());
  assert(!configuration_signature_matches(sig, "abc\n"));

  assert(write_configuration_signature(sig, "abc\n"));
  assert(read_configuration_signature(sig) == "abc\n");
  assert(configuration_signature_matches(sig, "abc\n"));
  assert(!configuration_signature_matches(sig, "abc"));

  assert(write_configuration_signature(sig, "replacement\n\n"));
  assert(read_configuration_signature(sig) == "replacement\n\n");

  const fs::path badPath = root / "as-file";
  {
    std::ofstream out(badPath);
    out << "not a directory";
  }
  const fs::path impossible = badPath / "child" / ".vix-config.sig";
  assert(!write_configuration_signature(impossible, "new\n"));
  assert(!read_configuration_signature(impossible).has_value());

  const fs::path blocked = root / "blocked.sig";
  assert(write_configuration_signature(blocked, "old\n"));
  fs::create_directories(root / "blocked.sig.tmp");
  assert(!write_configuration_signature(blocked, "new\n"));
  assert(read_configuration_signature(blocked) == "old\n");

  return 0;
}
