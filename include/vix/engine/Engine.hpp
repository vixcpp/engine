/**
 * @file Engine.hpp
 *
 * Public Vix Engine entry point.
 */
#ifndef VIX_ENGINE_ENGINE_HPP
#define VIX_ENGINE_ENGINE_HPP

#include <vix/engine/ExecutionResult.hpp>
#include <vix/engine/Options.hpp>

namespace vix::engine
{
  /**
   * @brief Reusable execution engine entry point.
   *
   * The first extraction phase exposes the boundary without taking over the
   * existing CLI build flow. Later phases can connect planning and execution
   * behind this stable class.
   */
  class Engine
  {
  public:
    ExecutionResult execute(const Options &options) const;
  };

} // namespace vix::engine

#endif
