/**
 * @file ExecutionResult.hpp
 *
 * Result model returned by Vix Engine.
 */
#ifndef VIX_ENGINE_EXECUTION_RESULT_HPP
#define VIX_ENGINE_EXECUTION_RESULT_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <vix/engine/Diagnostic.hpp>
#include <vix/engine/Event.hpp>

namespace vix::engine
{
  /**
   * @brief Final status for an engine execution request.
   */
  enum class ExecutionStatus
  {
    NotStarted,
    Succeeded,
    Failed,
    Unsupported
  };

  /**
   * @brief Structured result returned by Engine::execute().
   */
  struct ExecutionResult
  {
    ExecutionStatus status{ExecutionStatus::NotStarted};
    int exitCode{0};
    std::uint64_t tasksRun{0};
    std::uint64_t tasksSkipped{0};
    std::vector<Diagnostic> diagnostics;
    std::vector<Event> events;

    bool success() const
    {
      return status == ExecutionStatus::Succeeded && exitCode == 0;
    }
  };

} // namespace vix::engine

#endif
