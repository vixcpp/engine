/**
 *
 *  @file Preset.hpp
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

#ifndef VIX_ENGINE_PRESET_HPP
#define VIX_ENGINE_PRESET_HPP

#include <optional>
#include <string>
#include <vector>

namespace vix::engine
{
  /**
   * @brief Embedded build preset description.
   */
  struct Preset
  {
    std::string name;         ///< Preset public name
    std::string generator;    ///< CMake generator name, usually "Ninja"
    std::string buildType;    ///< CMake build type, such as Debug or Release
    std::string buildDirName; ///< Build directory name associated with the preset

    /**
     * @brief Check whether the preset contains the fields Vix requires.
     */
    bool valid() const;
  };

  /**
   * @brief Return built-in Vix presets in deterministic order.
   */
  std::vector<Preset> builtin_presets();

  /**
   * @brief Resolve a built-in Vix preset by name.
   */
  std::optional<Preset> resolve_builtin_preset(const std::string &name);

} // namespace vix::engine

#endif
