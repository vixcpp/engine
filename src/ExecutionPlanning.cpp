/**
 * @file ExecutionPlanning.cpp
 *
 * Pure execution plan layout assembly for Vix Engine.
 */
#include <vix/engine/ExecutionPlanning.hpp>

#include <utility>

namespace vix::engine
{
  bool ExecutionPlanLayoutResult::success() const
  {
    return error.empty() && plan.valid();
  }

  ExecutionPlanLayoutResult make_execution_plan_layout(
      const ExecutionPlanLayoutOptions &options)
  {
    ExecutionPlanLayoutResult result;

    if (options.userProjectDir.empty())
    {
      result.error = "user project directory is required";
      return result;
    }

    if (options.cmakeSourceDir.empty())
    {
      result.error = "CMake source directory is required";
      return result;
    }

    if (!options.preset.valid())
    {
      result.error = "valid preset is required";
      return result;
    }

    if (options.preset.buildDirName.empty())
    {
      result.error = "preset build directory name is required";
      return result;
    }

    ExecutionPlan plan;
    plan.userProjectDir = options.userProjectDir.lexically_normal();
    plan.cmakeSourceDir = options.cmakeSourceDir.lexically_normal();
    plan.projectDir = plan.userProjectDir;
    plan.defaultTargetName = options.defaultTargetName;
    plan.generatedFromVixApp = options.generatedFromVixApp;
    plan.preset = options.preset;

    if (!options.targetTriple.empty())
    {
      plan.buildDir =
          plan.userProjectDir /
          (plan.preset.buildDirName + "-" + options.targetTriple);
    }
    else
    {
      plan.buildDir = plan.userProjectDir / plan.preset.buildDirName;
    }

    plan.buildDir = plan.buildDir.lexically_normal();
    plan.configureLog = plan.buildDir / "configure.log";
    plan.buildLog = plan.buildDir / "build.log";
    plan.sigFile = plan.buildDir / ".vix-config.sig";
    plan.toolchainFile = plan.buildDir / "vix-toolchain.cmake";

    result.plan = std::move(plan);
    return result;
  }
} // namespace vix::engine
