/**
 *
 *  @file CMakeConfiguration.hpp
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
#ifndef VIX_ENGINE_CMAKE_CONFIGURATION_HPP
#define VIX_ENGINE_CMAKE_CONFIGURATION_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vix::engine
{
  using CMakeVariable = std::pair<std::string, std::string>;

  struct CMakeConfigurationOptions
  {
    std::string buildType;
    std::string targetTriple;

    bool linkStatic{false};
    bool withSqlite{false};
    bool withMySql{false};
    bool warningCheck{false};

    std::filesystem::path toolchainFile;
    std::filesystem::path globalPackagesFile;
    std::filesystem::path sdkConfigDir;

    std::optional<std::string> launcher;
    std::optional<std::string> fastLinkerFlag;
  };

  std::vector<CMakeVariable> make_cmake_variables(
      const CMakeConfigurationOptions &options);
} // namespace vix::engine

#endif
