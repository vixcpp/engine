/**
 * @file BuildParallelism.hpp
 * @brief Interactive-safe defaults for build parallelism.
 */
#ifndef VIX_ENGINE_BUILD_PARALLELISM_HPP
#define VIX_ENGINE_BUILD_PARALLELISM_HPP

#include <algorithm>
#include <thread>

namespace vix::engine
{
  /**
   * Return the automatic build concurrency for an interactive workstation.
   *
   * Explicit -j/--jobs values deliberately bypass this policy.  Automatic
   * builds retain one quarter of the available hardware threads (at least one
   * thread) for the editor, terminal, and operating system.  This avoids
   * treating all logical CPUs as build-only capacity.
   */
  inline int recommended_build_jobs(unsigned int hardwareThreads) noexcept
  {
    if (hardwareThreads == 0)
      return 1;

    hardwareThreads = std::min(hardwareThreads, 64U);

    if (hardwareThreads <= 2)
      return 1;

    const unsigned int reserved = std::max(1U, hardwareThreads / 4U);
    return static_cast<int>(hardwareThreads - reserved);
  }

  inline int default_build_jobs() noexcept
  {
    return recommended_build_jobs(std::thread::hardware_concurrency());
  }
} // namespace vix::engine

#endif // VIX_ENGINE_BUILD_PARALLELISM_HPP
