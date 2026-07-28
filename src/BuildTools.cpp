/**
 *
 *  @file BuildTools.cpp
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
#include <vix/engine/BuildTools.hpp>
#include "ExecutableLookup.hpp"

namespace vix::engine
{
  bool CompilerLauncherSelection::selected() const
  {
    return status == ToolSelectionStatus::Selected && executable.has_value();
  }

  bool FastLinkerSelection::selected() const
  {
    return status == ToolSelectionStatus::Selected && flag.has_value();
  }

  std::string to_string(LinkerMode mode)
  {
    switch (mode)
    {
    case LinkerMode::Auto:
      return "auto";
    case LinkerMode::Default:
      return "default";
    case LinkerMode::Mold:
      return "mold";
    case LinkerMode::Lld:
      return "lld";
    }

    return "auto";
  }

  std::string to_string(LauncherMode mode)
  {
    switch (mode)
    {
    case LauncherMode::Auto:
      return "auto";
    case LauncherMode::None:
      return "none";
    case LauncherMode::Sccache:
      return "sccache";
    case LauncherMode::Ccache:
      return "ccache";
    }

    return "auto";
  }

  CompilerLauncherSelection select_compiler_launcher(LauncherMode mode)
  {
    CompilerLauncherSelection selection;
    selection.requested = mode;

    switch (mode)
    {
    case LauncherMode::None:
      selection.status = ToolSelectionStatus::Disabled;
      selection.reason = "compiler launcher disabled";
      return selection;

    case LauncherMode::Sccache:
      if (detail::executable_on_path("sccache"))
      {
        selection.status = ToolSelectionStatus::Selected;
        selection.executable = "sccache";
        return selection;
      }
      selection.reason = "sccache not found on PATH";
      return selection;

    case LauncherMode::Ccache:
      if (detail::executable_on_path("ccache"))
      {
        selection.status = ToolSelectionStatus::Selected;
        selection.executable = "ccache";
        return selection;
      }
      selection.reason = "ccache not found on PATH";
      return selection;

    case LauncherMode::Auto:
    default:
      if (detail::executable_on_path("sccache"))
      {
        selection.status = ToolSelectionStatus::Selected;
        selection.executable = "sccache";
        return selection;
      }
      if (detail::executable_on_path("ccache"))
      {
        selection.status = ToolSelectionStatus::Selected;
        selection.executable = "ccache";
        return selection;
      }
      selection.reason = "no supported compiler launcher found on PATH";
      return selection;
    }
  }

  FastLinkerSelection select_fast_linker(LinkerMode mode)
  {
    FastLinkerSelection selection;
    selection.requested = mode;

#ifdef _WIN32
    if (mode == LinkerMode::Default)
    {
      selection.status = ToolSelectionStatus::Disabled;
      selection.reason = "default linker requested";
    }
    else
    {
      selection.reason = "fast linker selection is unavailable on Windows";
    }
    return selection;
#else
    const bool hasMold = detail::executable_on_path("mold");
    const bool hasLdLld = detail::executable_on_path("ld.lld");

    if (mode == LinkerMode::Default)
    {
      selection.status = ToolSelectionStatus::Disabled;
      selection.reason = "default linker requested";
      return selection;
    }

    if (mode == LinkerMode::Mold)
    {
      if (hasMold)
      {
        selection.status = ToolSelectionStatus::Selected;
        selection.flag = "-fuse-ld=mold";
        return selection;
      }
      selection.reason = "mold not found on PATH";
      return selection;
    }

    if (mode == LinkerMode::Lld)
    {
      if (hasLdLld)
      {
        selection.status = ToolSelectionStatus::Selected;
        selection.flag = "-fuse-ld=lld";
        return selection;
      }
      selection.reason = "ld.lld not found on PATH";
      return selection;
    }

    if (hasMold)
    {
      selection.status = ToolSelectionStatus::Selected;
      selection.flag = "-fuse-ld=mold";
      return selection;
    }
    if (hasLdLld)
    {
      selection.status = ToolSelectionStatus::Selected;
      selection.flag = "-fuse-ld=lld";
      return selection;
    }

    selection.reason = "no supported fast linker found on PATH";
    return selection;
#endif
  }

  std::optional<std::string> detect_compiler_launcher(LauncherMode mode)
  {
    return select_compiler_launcher(mode).executable;
  }

  std::optional<std::string> detect_fast_linker_flag(LinkerMode mode)
  {
    return select_fast_linker(mode).flag;
  }
} // namespace vix::engine
