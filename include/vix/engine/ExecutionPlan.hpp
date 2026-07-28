/**
 * @file ExecutionPlan.hpp
 *
 * Minimal execution-plan model for Vix Engine.
 */
#ifndef VIX_ENGINE_EXECUTION_PLAN_HPP
#define VIX_ENGINE_EXECUTION_PLAN_HPP

#include <string>
#include <vector>

#include <vix/engine/BuildTask.hpp>

namespace vix::engine
{
  /**
   * @brief A data-only plan describing work the engine may execute.
   */
  struct ExecutionPlan
  {
    std::string id;
    std::vector<BuildTask> tasks;
  };

} // namespace vix::engine

#endif
