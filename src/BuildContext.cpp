/**
 *
 *  @file BuildContext.cpp
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

#include <vix/engine/BuildContext.hpp>

namespace vix::engine
{
  namespace
  {
    static std::string fallback_project_name(const ExecutionPlan &plan)
    {
      if (!plan.projectDir.empty())
        return plan.projectDir.filename().string();

      return plan.userProjectDir.filename().string();
    }
  } // namespace

  std::string default_build_target_name(
      const BuildTargetOptions &options,
      const ExecutionPlan &plan)
  {
    return default_build_target_name(options.buildTarget, plan);
  }

  std::string default_graph_target_name(
      const BuildTargetOptions &options,
      const ExecutionPlan &plan)
  {
    return default_graph_target_name(options.buildTarget, plan);
  }

  std::filesystem::path default_project_executable_path(
      const BuildTargetOptions &options,
      const ExecutionPlan &plan)
  {
    return default_project_executable_path(options.buildTarget, plan);
  }

  std::string default_build_target_name(
      std::string_view buildTarget,
      const ExecutionPlan &plan)
  {
    if (!buildTarget.empty())
      return std::string(buildTarget);

    if (!plan.defaultTargetName.empty())
      return plan.defaultTargetName;

    return fallback_project_name(plan);
  }

  std::string default_graph_target_name(
      std::string_view buildTarget,
      const ExecutionPlan &plan)
  {
    return default_build_target_name(buildTarget, plan);
  }

  std::filesystem::path default_project_executable_path(
      std::string_view buildTarget,
      const ExecutionPlan &plan)
  {
    const std::string target = default_build_target_name(buildTarget, plan);

#ifdef _WIN32
    return plan.buildDir / (target + ".exe");
#else
    return plan.buildDir / target;
#endif
  }

} // namespace vix::engine
