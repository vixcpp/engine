/**
 *
 *  @file Preset.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Engine build preset model
 *
 */

#include <vix/engine/Preset.hpp>

#include <array>

namespace vix::engine
{
  bool Preset::valid() const
  {
    return !name.empty() &&
           !generator.empty() &&
           !buildType.empty() &&
           !buildDirName.empty();
  }

  std::vector<Preset> builtin_presets()
  {
    static const std::array<Preset, 3> presets{
        Preset{"dev", "Ninja", "Debug", "build-dev"},
        Preset{"dev-ninja", "Ninja", "Debug", "build-ninja"},
        Preset{"release", "Ninja", "Release", "build-release"}};

    return {presets.begin(), presets.end()};
  }

  std::optional<Preset> resolve_builtin_preset(const std::string &name)
  {
    for (const Preset &preset : builtin_presets())
    {
      if (preset.name == name)
        return preset;
    }

    return std::nullopt;
  }

} // namespace vix::engine
