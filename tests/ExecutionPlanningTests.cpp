#include <vix/engine/ExecutionPlanning.hpp>

#include <cassert>
#include <filesystem>

namespace
{
  namespace fs = std::filesystem;
  using namespace vix::engine;

  Preset preset()
  {
    return Preset{"dev-ninja", "Ninja", "Debug", "build-ninja"};
  }
}

int main()
{
  {
    ExecutionPlanLayoutOptions options;
    auto result = make_execution_plan_layout(options);
    assert(!result.success());
    assert(!result.error.empty());
  }

  {
    ExecutionPlanLayoutOptions options;
    options.cmakeSourceDir = "/tmp/source";
    options.preset = preset();
    assert(!make_execution_plan_layout(options).success());

    options.userProjectDir = "/tmp/project";
    options.cmakeSourceDir.clear();
    assert(!make_execution_plan_layout(options).success());

    options.cmakeSourceDir = "/tmp/source";
    options.preset = {};
    assert(!make_execution_plan_layout(options).success());
  }

  const fs::path before = fs::temp_directory_path() / "vix-engine-layout-not-created";
  std::error_code ec;
  fs::remove_all(before, ec);

  {
    ExecutionPlanLayoutOptions options;
    options.userProjectDir = before / ".." / "vix-engine-layout-not-created";
    options.cmakeSourceDir = options.userProjectDir / "." / "generated";
    options.defaultTargetName = "";
    options.generatedFromVixApp = true;
    options.preset = preset();

    const auto result = make_execution_plan_layout(options);
    assert(result.success());
    const ExecutionPlan &plan = result.plan;
    assert(plan.userProjectDir == before.lexically_normal());
    assert(plan.cmakeSourceDir == (before / "generated").lexically_normal());
    assert(plan.projectDir == plan.userProjectDir);
    assert(plan.defaultTargetName.empty());
    assert(plan.generatedFromVixApp);
    assert(plan.preset.name == "dev-ninja");
    assert(plan.buildDir == before / "build-ninja");
    assert(plan.configureLog == before / "build-ninja" / "configure.log");
    assert(plan.buildLog == before / "build-ninja" / "build.log");
    assert(plan.sigFile == before / "build-ninja" / ".vix-config.sig");
    assert(plan.toolchainFile == before / "build-ninja" / "vix-toolchain.cmake");
    assert(!fs::exists(before, ec));
    assert(make_execution_plan_layout(options).plan.buildDir == plan.buildDir);
  }

  {
    ExecutionPlanLayoutOptions options;
    options.userProjectDir = "/tmp/project";
    options.cmakeSourceDir = "/tmp/project";
    options.defaultTargetName = "app";
    options.preset = preset();
    options.targetTriple = "aarch64-linux-gnu";

    const auto result = make_execution_plan_layout(options);
    assert(result.success());
    assert(result.plan.buildDir == "/tmp/project/build-ninja-aarch64-linux-gnu");
    assert(result.plan.configureLog == "/tmp/project/build-ninja-aarch64-linux-gnu/configure.log");
    assert(result.plan.sigFile == "/tmp/project/build-ninja-aarch64-linux-gnu/.vix-config.sig");
    assert(result.plan.toolchainFile == "/tmp/project/build-ninja-aarch64-linux-gnu/vix-toolchain.cmake");
  }

  return 0;
}
