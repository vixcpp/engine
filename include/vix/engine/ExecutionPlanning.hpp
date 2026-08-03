/**
 * @file ExecutionPlanning.hpp
 *
 * Pure execution plan layout assembly for Vix Engine.
 */
#ifndef VIX_ENGINE_EXECUTION_PLANNING_HPP
#define VIX_ENGINE_EXECUTION_PLANNING_HPP

#include <filesystem>
#include <string>

#include <vix/engine/ExecutionPlan.hpp>
#include <vix/engine/Preset.hpp>
#include <vix/engine/SanitizerMode.hpp>

namespace vix::engine
{
  struct ExecutionPlanLayoutOptions
  {
    std::filesystem::path userProjectDir;
    std::filesystem::path cmakeSourceDir;

    std::string defaultTargetName;
    bool generatedFromVixApp{false};

    Preset preset;
    std::string targetTriple;

    /**
     * @brief Sanitizer variant used to isolate build artifacts.
     */
    SanitizerMode sanitizerMode{SanitizerMode::None};
  };

  struct ExecutionPlanLayoutResult
  {
    ExecutionPlan plan;
    std::string error;

    [[nodiscard]] bool success() const;
  };

  ExecutionPlanLayoutResult make_execution_plan_layout(
      const ExecutionPlanLayoutOptions &options);
} // namespace vix::engine

#endif
