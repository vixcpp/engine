/**
 *
 *  @file BuildTools.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Build graph task model
 *
 */
#ifndef VIX_ENGINE_BUILD_TOOLS_HPP
#define VIX_ENGINE_BUILD_TOOLS_HPP

#include <optional>
#include <string>

namespace vix::engine
{
  enum class LinkerMode
  {
    Auto,
    Default,
    Mold,
    Lld
  };

  enum class LauncherMode
  {
    Auto,
    None,
    Sccache,
    Ccache
  };

  struct BuildToolPreferences
  {
    LinkerMode linker{LinkerMode::Auto};
    LauncherMode launcher{LauncherMode::Auto};
  };

  enum class ToolSelectionStatus
  {
    Selected,
    Disabled,
    Unavailable
  };

  struct CompilerLauncherSelection
  {
    ToolSelectionStatus status{ToolSelectionStatus::Unavailable};
    LauncherMode requested{LauncherMode::Auto};
    std::optional<std::string> executable;
    std::string reason;

    [[nodiscard]] bool selected() const;
  };

  struct FastLinkerSelection
  {
    ToolSelectionStatus status{ToolSelectionStatus::Unavailable};
    LinkerMode requested{LinkerMode::Auto};
    std::optional<std::string> flag;
    std::string reason;

    [[nodiscard]] bool selected() const;
  };

  std::string to_string(LinkerMode mode);
  std::string to_string(LauncherMode mode);

  CompilerLauncherSelection select_compiler_launcher(LauncherMode mode);
  FastLinkerSelection select_fast_linker(LinkerMode mode);

  std::optional<std::string> detect_compiler_launcher(LauncherMode mode);
  std::optional<std::string> detect_fast_linker_flag(LinkerMode mode);
} // namespace vix::engine

#endif
