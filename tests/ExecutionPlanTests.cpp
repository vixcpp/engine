/**
 *
 *  @file ExecutionPlanTests.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Execution plan, preset and build-context tests
 *
 */

#include <vix/engine.hpp>

#include <cassert>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
  namespace fs = std::filesystem;
  using namespace vix::engine;

  ExecutionPlan minimal_plan()
  {
    ExecutionPlan plan;
    plan.userProjectDir = "/tmp/project";
    plan.cmakeSourceDir = "/tmp/project";
    plan.projectDir = "/tmp/project";
    plan.preset = Preset{"dev-ninja", "Ninja", "Debug", "build-ninja"};
    plan.buildDir = "/tmp/project/build-ninja";
    return plan;
  }
}

int main()
{
  {
    const std::vector<Preset> presets = builtin_presets();
    assert(presets.size() == 3);
    assert(presets[0].name == "dev");
    assert(presets[0].generator == "Ninja");
    assert(presets[0].buildType == "Debug");
    assert(presets[0].buildDirName == "build-dev");
    assert(presets[1].name == "dev-ninja");
    assert(presets[1].generator == "Ninja");
    assert(presets[1].buildType == "Debug");
    assert(presets[1].buildDirName == "build-ninja");
    assert(presets[2].name == "release");
    assert(presets[2].generator == "Ninja");
    assert(presets[2].buildType == "Release");
    assert(presets[2].buildDirName == "build-release");

    assert(resolve_builtin_preset("dev").has_value());
    assert(resolve_builtin_preset("dev-ninja").has_value());
    assert(resolve_builtin_preset("release").has_value());
    assert(!resolve_builtin_preset("invalid-preset").has_value());
    assert(!resolve_builtin_preset("").has_value());

    assert((Preset{"x", "Ninja", "Debug", "build-x"}.valid()));
    assert((!Preset{"", "Ninja", "Debug", "build-x"}.valid()));
    assert((!Preset{"x", "", "Debug", "build-x"}.valid()));
    assert((!Preset{"x", "Ninja", "", "build-x"}.valid()));
    assert((!Preset{"x", "Ninja", "Debug", ""}.valid()));
  }

  {
    static_assert(std::is_copy_constructible_v<Preset>);
    static_assert(std::is_move_constructible_v<Preset>);
    static_assert(std::is_copy_constructible_v<ExecutionPlan>);
    static_assert(std::is_move_constructible_v<ExecutionPlan>);

    ExecutionPlan empty;
    assert(!empty.valid());

    ExecutionPlan plan = minimal_plan();
    assert(plan.valid());
    assert(plan.configureLog.empty());
    assert(plan.buildLog.empty());
    assert(plan.sigFile.empty());
    assert(plan.toolchainFile.empty());
    assert(plan.sdkConfigDir.empty());
    assert(plan.cmakeVars.empty());
    assert(plan.signature.empty());
    assert(!plan.launcher.has_value());
    assert(!plan.fastLinkerFlag.has_value());
    assert(plan.projectFingerprint.empty());

    ExecutionPlan missingUser = plan;
    missingUser.userProjectDir.clear();
    assert(!missingUser.valid());

    ExecutionPlan missingSource = plan;
    missingSource.cmakeSourceDir.clear();
    assert(!missingSource.valid());

    ExecutionPlan missingBuild = plan;
    missingBuild.buildDir.clear();
    assert(!missingBuild.valid());

    ExecutionPlan missingPreset = plan;
    missingPreset.preset = Preset{};
    assert(!missingPreset.valid());

    plan.generatedFromVixApp = true;
    plan.defaultTargetName = "hello";
    plan.cmakeVars.push_back({"CMAKE_BUILD_TYPE", "Debug"});
    plan.launcher = "ccache";
    plan.fastLinkerFlag = "-fuse-ld=lld";
    ExecutionPlan copy = plan;
    assert(copy.generatedFromVixApp);
    assert(copy.defaultTargetName == "hello");
    assert(copy.cmakeVars.size() == 1);
    assert(copy.launcher == "ccache");
    assert(copy.fastLinkerFlag == "-fuse-ld=lld");
  }

  {
    ExecutionPlan plan = minimal_plan();
    BuildTargetOptions options;

    options.buildTarget = "explicit";
    assert(default_build_target_name(options, plan) == "explicit");
    assert(default_graph_target_name(options, plan) == "explicit");
#ifdef _WIN32
    const fs::path explicitExe = "/tmp/project/build-ninja/explicit.exe";
#else
    const fs::path explicitExe = "/tmp/project/build-ninja/explicit";
#endif
    assert(default_project_executable_path(options, plan) == explicitExe);

    options.buildTarget.clear();
    plan.defaultTargetName = "default-target";
    assert(default_build_target_name(options, plan) == "default-target");
    assert(default_graph_target_name(options, plan) == "default-target");

    plan.defaultTargetName.clear();
    assert(default_build_target_name(options, plan) == "project");
    assert(default_graph_target_name(options, plan) == "project");

    plan.projectDir.clear();
    plan.userProjectDir = "/tmp/user-project";
    assert(default_build_target_name(options, plan) == "user-project");
    assert(default_graph_target_name(options, plan) == "user-project");
    assert(default_build_target_name("direct", plan) == "direct");
    assert(default_graph_target_name("direct", plan) == "direct");
  }

  return 0;
}
