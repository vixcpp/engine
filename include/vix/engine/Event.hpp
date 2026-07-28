/**
 * @file Event.hpp
 *
 * Structured execution events produced by Vix Engine.
 */
#ifndef VIX_ENGINE_EVENT_HPP
#define VIX_ENGINE_EVENT_HPP

#include <string>

namespace vix::engine
{
  /**
   * @brief Coarse engine event category.
   */
  enum class EventKind
  {
    Message,
    PlanCreated,
    TaskStarted,
    TaskFinished
  };

  /**
   * @brief Data-only event for engine consumers.
   *
   * Consumers such as the CLI decide how, when, and whether to render events.
   */
  struct Event
  {
    EventKind kind{EventKind::Message};
    std::string message;
    std::string taskId;
  };

} // namespace vix::engine

#endif
