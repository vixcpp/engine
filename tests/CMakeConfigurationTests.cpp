#include <vix/engine/CMakeConfiguration.hpp>

#include "EnvGuard.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace
{
  using namespace vix::engine;

  bool has_system_ar()
  {
#ifdef _WIN32
    return false;
#else
    std::error_code ec;
    return std::filesystem::exists("/usr/bin/ar", ec) && !ec;
#endif
  }

  bool has_system_ranlib()
  {
#ifdef _WIN32
    return false;
#else
    std::error_code ec;
    return std::filesystem::exists("/usr/bin/ranlib", ec) && !ec;
#endif
  }

  std::vector<CMakeVariable> base_debug()
  {
    std::vector<CMakeVariable> vars;
    if (has_system_ar())
      vars.emplace_back("CMAKE_AR", "/usr/bin/ar");
    vars.emplace_back("CMAKE_BUILD_TYPE", "Debug");
    vars.emplace_back("CMAKE_EXPORT_COMPILE_COMMANDS", "ON");
    if (has_system_ranlib())
      vars.emplace_back("CMAKE_RANLIB", "/usr/bin/ranlib");
    return vars;
  }
}

int main()
{
  using vix::engine::tests::EnvGuard;

  EnvGuard path("PATH");
  EnvGuard cxx("CXX");
  EnvGuard cc("CC");
  path.set("");
  cxx.unset();
  cc.unset();

  {
    CMakeConfigurationOptions options;
    options.buildType = "Debug";
    assert(make_cmake_variables(options) == base_debug());
    assert(make_cmake_variables(options) == make_cmake_variables(options));
  }

  {
    CMakeConfigurationOptions options;
    options.buildType = "Release";

    std::vector<CMakeVariable> expected;
    if (has_system_ar())
      expected.emplace_back("CMAKE_AR", "/usr/bin/ar");
    expected.emplace_back("CMAKE_BUILD_TYPE", "Release");
    expected.emplace_back("CMAKE_EXPORT_COMPILE_COMMANDS", "ON");
    if (has_system_ranlib())
      expected.emplace_back("CMAKE_RANLIB", "/usr/bin/ranlib");

    assert(make_cmake_variables(options) == expected);
  }

  {
    CMakeConfigurationOptions options;
    options.buildType = "Debug";
    options.linkStatic = true;
    options.withSqlite = true;
    options.withMySql = true;
    options.warningCheck = true;
    options.targetTriple = "aarch64-linux-gnu";
    options.toolchainFile = "/tmp/toolchain.cmake";
    options.sdkConfigDir = "/tmp/sdk/lib/cmake/Vix";
    options.globalPackagesFile = "/tmp/global packages.cmake";
    options.launcher = "ccache";
    options.fastLinkerFlag = "-fuse-ld=mold";

    std::vector<CMakeVariable> expected;
    if (has_system_ar())
      expected.emplace_back("CMAKE_AR", "/usr/bin/ar");
    expected.emplace_back("CMAKE_BUILD_TYPE", "Debug");
    expected.emplace_back("CMAKE_CXX_COMPILER", "c++");
    expected.emplace_back("CMAKE_CXX_COMPILER_LAUNCHER", "ccache");
    expected.emplace_back(
        "CMAKE_CXX_FLAGS",
        "-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wformat=2 -Wold-style-cast -Woverloaded-virtual");
    expected.emplace_back("CMAKE_C_COMPILER_LAUNCHER", "ccache");
    expected.emplace_back("CMAKE_EXE_LINKER_FLAGS", "-fuse-ld=mold");
    expected.emplace_back("CMAKE_EXPORT_COMPILE_COMMANDS", "ON");
    expected.emplace_back("CMAKE_MODULE_LINKER_FLAGS", "-fuse-ld=mold");
    if (has_system_ranlib())
      expected.emplace_back("CMAKE_RANLIB", "/usr/bin/ranlib");
    expected.emplace_back("CMAKE_SHARED_LINKER_FLAGS", "-fuse-ld=mold");
    expected.emplace_back("CMAKE_TOOLCHAIN_FILE", "/tmp/toolchain.cmake");
    expected.emplace_back("VIX_DB_REQUIRE_MYSQL", "ON");
    expected.emplace_back("VIX_DB_REQUIRE_SQLITE", "ON");
    expected.emplace_back("VIX_DB_USE_MYSQL", "ON");
    expected.emplace_back("VIX_DB_USE_SQLITE", "ON");
    expected.emplace_back("VIX_ENABLE_DB", "ON");
    expected.emplace_back("VIX_ENABLE_DB", "ON");
    expected.emplace_back("VIX_ENABLE_WARNINGS", "ON");
    expected.emplace_back("VIX_LINK_STATIC", "ON");
    expected.emplace_back("VIX_TARGET_TRIPLE", "aarch64-linux-gnu");

    assert(make_cmake_variables(options) == expected);
    for (const auto &var : make_cmake_variables(options))
      assert(var.first != "globalPackagesFile");
  }

  {
    CMakeConfigurationOptions options;
    options.buildType = "Debug";
    options.dependencyEnvironmentMode = DependencyEnvironmentMode::ManagedSdk;
    options.sdkConfigDir = "/tmp/sdk/lib/cmake/Vix";

    std::vector<CMakeVariable> expected = base_debug();
    expected.emplace_back("Vix_DIR", "/tmp/sdk/lib/cmake/Vix");
    expected.emplace_back("vix_DIR", "/tmp/sdk/lib/cmake/Vix");
    std::sort(expected.begin(), expected.end());

    assert(make_cmake_variables(options) == expected);
  }

  {
    CMakeConfigurationOptions options;
    options.buildType = "Debug";
    options.launcher = "";
    options.fastLinkerFlag = "";
    assert(make_cmake_variables(options) == base_debug());
  }

  {
    CMakeConfigurationOptions options;
    options.buildType = "Debug";
    cxx.set("my-clang++");
    cc.set("my-clang");
    options.warningCheck = true;

    std::vector<CMakeVariable> expected;
    if (has_system_ar())
      expected.emplace_back("CMAKE_AR", "/usr/bin/ar");
    expected.emplace_back("CMAKE_BUILD_TYPE", "Debug");
    expected.emplace_back(
        "CMAKE_CXX_FLAGS",
        "-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wformat=2 -Wold-style-cast -Woverloaded-virtual -Wlogical-op-parentheses -Wunreachable-code");
    expected.emplace_back("CMAKE_EXPORT_COMPILE_COMMANDS", "ON");
    if (has_system_ranlib())
      expected.emplace_back("CMAKE_RANLIB", "/usr/bin/ranlib");
    expected.emplace_back("VIX_ENABLE_WARNINGS", "ON");

    assert(make_cmake_variables(options) == expected);
  }

  return 0;
}
