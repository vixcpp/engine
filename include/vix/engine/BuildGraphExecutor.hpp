/**
 *
 *  @file BuildGraphExecutor.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Target-aware build graph executor
 *
 */

#ifndef VIX_ENGINE_BUILD_GRAPH_EXECUTOR_HPP
#define VIX_ENGINE_BUILD_GRAPH_EXECUTOR_HPP

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <cstddef>

#include <vix/engine/BuildGraph.hpp>
#include <vix/engine/BuildScheduler.hpp>

namespace vix::engine
{
  namespace fs = std::filesystem;

  /**
   * @brief Status returned by the target-aware graph executor.
   */
  enum class BuildGraphExecutorStatus
  {
    Success,
    UpToDate,
    DelegatedToNinja,
    InvalidRequest,
    InvalidGraph,
    UnsupportedTarget,
    CompileFailed,
    NinjaFailed,
    CacheFailed
  };

  /**
   * @brief Convert a graph executor status to a stable string.
   *
   * @param status Executor status
   * @return Stable lowercase string
   */
  const char *to_string(BuildGraphExecutorStatus status);

  /**
   * @brief Options used by the target-aware graph executor.
   */
  struct BuildGraphExecutorOptions
  {
    fs::path buildDir;  ///< Build directory containing Ninja and graph outputs
    std::string target; ///< Requested output target
    int jobs{0};        ///< Compile scheduler jobs. 0 means auto.

    /**
     * @brief Allow delegation to Ninja when direct graph execution is unsafe.
     */
    bool allowNinjaFallback{true};

    /**
     * @brief Maximum dirty compile tasks allowed before delegating to Ninja.
     *
     * 0 means no artificial limit.
     */
    std::size_t maxGraphDirtyCompileTasks{0};
  };

  /**
   * @brief Neutral Ninja target execution request.
   */
  struct BuildGraphExecutorNinjaRequest
  {
    fs::path buildDir;                ///< Build directory used by Ninja
    std::string target;               ///< Requested Ninja target
    std::vector<std::string> command; ///< Structured command argv
  };

  /**
   * @brief Neutral result returned by a Ninja fallback callback.
   */
  struct BuildGraphExecutorNinjaResult
  {
    bool started{false};        ///< true if Ninja was started
    int exitCode{0};            ///< Normalized exit code
    bool producedOutput{false}; ///< true if output was produced

    std::string displayCommand; ///< Diagnostic-only display command
    std::string output;         ///< Captured or summarized output
    std::string errorMessage;   ///< Startup or execution error

    /**
     * @brief Check whether Ninja started and exited successfully.
     */
    bool success() const;
  };

  /**
   * @brief Structured graph executor event kind.
   */
  enum class BuildGraphExecutorEventKind
  {
    ResolvingTarget,
    TargetResolved,
    SelectingTasks,
    CompilingTask,
    CacheHit,
    DelegatingToNinja,
    RunningNinja,
    Completed
  };

  /**
   * @brief Structured graph executor event.
   */
  struct BuildGraphExecutorEvent
  {
    BuildGraphExecutorEventKind kind{BuildGraphExecutorEventKind::Completed};

    std::string message; ///< Neutral event message
    std::string taskId;  ///< Related task id, when any
    std::string target;  ///< Related target, when any

    std::size_t current{0}; ///< Current operation index, when known
    std::size_t total{0};   ///< Total operation count, when known
  };

  using BuildGraphCompileExecutor =
      std::function<BuildTaskResult(BuildTask &)>;

  using BuildGraphNinjaExecutor =
      std::function<BuildGraphExecutorNinjaResult(
          const BuildGraphExecutorNinjaRequest &)>;

  using BuildGraphExecutorEventSink =
      std::function<void(const BuildGraphExecutorEvent &)>;

  /**
   * @brief Injected dependencies for graph execution.
   */
  struct BuildGraphExecutorDependencies
  {
    BuildGraphCompileExecutor executeCompileTask; ///< Compile task executor
    BuildGraphNinjaExecutor executeNinjaTarget;   ///< Ninja fallback executor
    BuildGraphExecutorEventSink onEvent;          ///< Optional event sink
    std::optional<fs::path> objectCacheRoot;      ///< Optional explicit cache root
  };

  /**
   * @brief Result returned by target-aware graph execution.
   */
  struct BuildGraphExecutorResult
  {
    bool ok{false}; ///< true when execution succeeded

    BuildGraphExecutorStatus status{BuildGraphExecutorStatus::InvalidRequest};

    std::string target; ///< Requested target
    std::string reason; ///< Neutral reason for fallback or failure

    bool usedGraph{false};    ///< true if graph analysis was used
    bool usedNinja{false};    ///< true if Ninja was executed
    bool usedFallback{false}; ///< true if execution delegated to Ninja

    std::size_t selectedTasks{0};        ///< Transitive task closure size
    std::size_t selectedCompileTasks{0}; ///< Compile tasks in closure
    std::size_t dirtyCompileTasks{0};    ///< Dirty compile tasks
    std::size_t executedCompileTasks{0}; ///< Executed compile tasks
    std::size_t skippedCompileTasks{0};  ///< Cache-skipped compile tasks

    int exitCode{0};    ///< Final exit code
    std::string output; ///< Accumulated neutral output

    /**
     * @brief Check whether the graph executor completed successfully.
     */
    bool success() const;
  };

  /**
   * @brief Target-aware build graph executor.
   *
   * The executor analyzes the build graph, compiles dirty object tasks through
   * an injected callback, reuses ObjectCache, and delegates unsafe or final
   * target execution to an injected Ninja callback.
   */
  class BuildGraphExecutor
  {
  public:
    /**
     * @brief Create an executor with options and default dependencies.
     *
     * The default compile executor uses vix::engine::process. Ninja execution
     * is intentionally not provided by default because live presentation
     * belongs to the caller.
     */
    explicit BuildGraphExecutor(BuildGraphExecutorOptions options);

    /**
     * @brief Create an executor with options and injected dependencies.
     */
    BuildGraphExecutor(
        BuildGraphExecutorOptions options,
        BuildGraphExecutorDependencies dependencies);

    /**
     * @brief Return executor options.
     */
    const BuildGraphExecutorOptions &options() const;

    /**
     * @brief Run the requested target through the graph executor.
     */
    BuildGraphExecutorResult run_target(BuildGraph &graph) const;

  private:
    BuildGraphExecutorOptions options_;
    BuildGraphExecutorDependencies dependencies_;
  };

} // namespace vix::engine

#endif
