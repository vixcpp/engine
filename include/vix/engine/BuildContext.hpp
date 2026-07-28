/**
 *
 *  @file BuildContext.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Shared engine build context helpers
 *
 */

#ifndef VIX_ENGINE_BUILD_CONTEXT_HPP
#define VIX_ENGINE_BUILD_CONTEXT_HPP

#include <filesystem>
#include <string>
#include <string_view>

#include <vix/engine/ExecutionPlan.hpp>
#include <vix/engine/Preset.hpp>

namespace vix::engine
{
  /**
   * @brief Minimal target-related options needed by target helpers.
   */
  struct BuildTargetOptions
  {
    std::string buildTarget; ///< Explicit target requested by the caller
  };

  /**
   * @brief Return the target name used by build execution.
   */
  std::string default_build_target_name(
      const BuildTargetOptions &options,
      const ExecutionPlan &plan);

  /**
   * @brief Return the target name used by graph execution.
   */
  std::string default_graph_target_name(
      const BuildTargetOptions &options,
      const ExecutionPlan &plan);

  /**
   * @brief Return the default executable path for the selected target.
   */
  std::filesystem::path default_project_executable_path(
      const BuildTargetOptions &options,
      const ExecutionPlan &plan);

  /**
   * @brief Return the target name used by build execution.
   */
  std::string default_build_target_name(
      std::string_view buildTarget,
      const ExecutionPlan &plan);

  /**
   * @brief Return the target name used by graph execution.
   */
  std::string default_graph_target_name(
      std::string_view buildTarget,
      const ExecutionPlan &plan);

  /**
   * @brief Return the default executable path for the selected target.
   */
  std::filesystem::path default_project_executable_path(
      std::string_view buildTarget,
      const ExecutionPlan &plan);

} // namespace vix::engine

#endif
