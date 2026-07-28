#include <vix/engine/BuildTools.hpp>

#include "EnvGuard.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <type_traits>

namespace
{
  namespace fs = std::filesystem;
  using namespace vix::engine;

  fs::path make_temp_dir(const std::string &name)
  {
    fs::path dir = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    assert(!ec);
    return dir;
  }

  void make_executable(const fs::path &path)
  {
    std::ofstream out(path);
    out << "#!/bin/sh\nexit 0\n";
    out.close();

    std::error_code ec;
    fs::permissions(
        path,
        fs::perms::owner_exec |
            fs::perms::owner_read |
            fs::perms::owner_write |
            fs::perms::group_exec |
            fs::perms::group_read |
            fs::perms::others_exec |
            fs::perms::others_read,
        fs::perm_options::replace,
        ec);
    assert(!ec);
  }
}

int main()
{
  using vix::engine::tests::EnvGuard;

  static_assert(std::is_copy_constructible_v<BuildToolPreferences>);
  static_assert(std::is_move_constructible_v<BuildToolPreferences>);
  static_assert(std::is_copy_constructible_v<CompilerLauncherSelection>);
  static_assert(std::is_move_constructible_v<CompilerLauncherSelection>);
  static_assert(std::is_copy_constructible_v<FastLinkerSelection>);
  static_assert(std::is_move_constructible_v<FastLinkerSelection>);

  assert(to_string(LinkerMode::Auto) == "auto");
  assert(to_string(LinkerMode::Default) == "default");
  assert(to_string(LinkerMode::Mold) == "mold");
  assert(to_string(LinkerMode::Lld) == "lld");
  assert(to_string(LauncherMode::Auto) == "auto");
  assert(to_string(LauncherMode::None) == "none");
  assert(to_string(LauncherMode::Sccache) == "sccache");
  assert(to_string(LauncherMode::Ccache) == "ccache");

  BuildToolPreferences preferences;
  assert(preferences.linker == LinkerMode::Auto);
  assert(preferences.launcher == LauncherMode::Auto);

  EnvGuard path("PATH");
  path.set("");

  {
    auto launcher = select_compiler_launcher(LauncherMode::None);
    assert(launcher.status == ToolSelectionStatus::Disabled);
    assert(!launcher.selected());
    assert(!launcher.executable.has_value());
    assert(!detect_compiler_launcher(LauncherMode::None).has_value());
  }

  {
    auto launcher = select_compiler_launcher(LauncherMode::Ccache);
    assert(launcher.status == ToolSelectionStatus::Unavailable);
    assert(!launcher.executable.has_value());

    launcher = select_compiler_launcher(LauncherMode::Sccache);
    assert(launcher.status == ToolSelectionStatus::Unavailable);
    assert(!launcher.executable.has_value());

    launcher = select_compiler_launcher(LauncherMode::Auto);
    assert(launcher.status == ToolSelectionStatus::Unavailable);
    assert(!launcher.executable.has_value());
  }

  const fs::path tools = make_temp_dir("vix-engine-build-tools tests");
  path.set(tools.string());

  make_executable(tools / "ccache");
  {
    auto launcher = select_compiler_launcher(LauncherMode::Ccache);
    assert(launcher.selected());
    assert(launcher.executable == "ccache");

    launcher = select_compiler_launcher(LauncherMode::Auto);
    assert(launcher.selected());
    assert(launcher.executable == "ccache");
  }

  make_executable(tools / "sccache");
  {
    auto launcher = select_compiler_launcher(LauncherMode::Sccache);
    assert(launcher.selected());
    assert(launcher.executable == "sccache");

    launcher = select_compiler_launcher(LauncherMode::Auto);
    assert(launcher.selected());
    assert(launcher.executable == "sccache");
  }

  {
    auto linker = select_fast_linker(LinkerMode::Default);
    assert(linker.status == ToolSelectionStatus::Disabled);
    assert(!linker.flag.has_value());
  }

  {
    path.set("");
    auto linker = select_fast_linker(LinkerMode::Mold);
#ifdef _WIN32
    assert(linker.status == ToolSelectionStatus::Unavailable);
#else
    assert(linker.status == ToolSelectionStatus::Unavailable);
#endif
    assert(!linker.flag.has_value());

    linker = select_fast_linker(LinkerMode::Lld);
    assert(!linker.flag.has_value());

    linker = select_fast_linker(LinkerMode::Auto);
    assert(!linker.flag.has_value());
  }

  {
    const fs::path linkerDir = make_temp_dir("vix-engine-linker-tests");
    path.set(linkerDir.string());
    make_executable(linkerDir / "ld.lld");
    make_executable(linkerDir / "lld");

    auto linker = select_fast_linker(LinkerMode::Lld);
#ifdef _WIN32
    assert(!linker.flag.has_value());
#else
    assert(linker.selected());
    assert(linker.flag == "-fuse-ld=lld");
#endif

    linker = select_fast_linker(LinkerMode::Auto);
#ifdef _WIN32
    assert(!linker.flag.has_value());
#else
    assert(linker.flag == "-fuse-ld=lld");
#endif

    make_executable(linkerDir / "mold");
    linker = select_fast_linker(LinkerMode::Mold);
#ifdef _WIN32
    assert(!linker.flag.has_value());
#else
    assert(linker.selected());
    assert(linker.flag == "-fuse-ld=mold");
#endif

    linker = select_fast_linker(LinkerMode::Auto);
#ifdef _WIN32
    assert(!linker.flag.has_value());
#else
    assert(linker.flag == "-fuse-ld=mold");
#endif
  }

  {
    const fs::path linkerDir = make_temp_dir("vix-engine-lld-name-only");
    path.set(linkerDir.string());
    make_executable(linkerDir / "lld");

    auto linker = select_fast_linker(LinkerMode::Lld);
    assert(!linker.flag.has_value());
  }

  return 0;
}
