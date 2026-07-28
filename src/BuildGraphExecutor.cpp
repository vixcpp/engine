/**
 *
 *  @file BuildGraphExecutor.cpp
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

#include <vix/engine/BuildGraphExecutor.hpp>

#include <algorithm>
#include <exception>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <vix/engine/DependencyFile.hpp>
#include <vix/engine/ObjectCache.hpp>
#include <vix/engine/Process.hpp>

namespace vix::engine
{
  namespace
  {
    static bool same_target_name(
        const fs::path &output,
        const std::string &target)
    {
      if (target.empty())
        return false;

      const fs::path targetPath(target);

      if (targetPath.is_absolute())
        return output.lexically_normal() == targetPath.lexically_normal();

      const std::string filename = output.filename().string();

#ifdef _WIN32
      if (filename == target)
        return true;

      if (filename == target + ".exe")
        return true;

      if (output.stem().string() == target)
        return true;
#else
      if (filename == target)
        return true;
#endif

      return false;
    }

    static BuildGraphExecutorResult make_failed_result(
        const BuildGraphExecutorOptions &options,
        BuildGraphExecutorStatus status,
        int exitCode,
        const std::string &reason)
    {
      BuildGraphExecutorResult result;
      result.ok = false;
      result.status = status;
      result.target = options.target;
      result.reason = reason;
      result.exitCode = exitCode;

      if (!reason.empty())
        result.output = reason + "\n";

      return result;
    }

    static void emit_event(
        const BuildGraphExecutorDependencies &dependencies,
        BuildGraphExecutorEventKind kind,
        const std::string &message,
        const std::string &target,
        const std::string &taskId = {})
    {
      if (!dependencies.onEvent)
        return;

      BuildGraphExecutorEvent event;
      event.kind = kind;
      event.message = message;
      event.target = target;
      event.taskId = taskId;
      dependencies.onEvent(event);
    }

    static std::unordered_map<std::string, std::string>
    make_output_to_task_map(const BuildGraph &graph)
    {
      std::unordered_map<std::string, std::string> out;

      for (const auto &kv : graph.tasks())
      {
        const BuildTask &task = kv.second;

        for (const std::string &outputId : task.outputs)
          out[outputId] = task.id;
      }

      return out;
    }

    static bool is_real_graph_target_task(const BuildTask &task)
    {
      return task.kind == BuildTaskKind::Link ||
             task.kind == BuildTaskKind::Archive;
    }

    static bool node_is_real_graph_output(const BuildNode &node)
    {
      return node.kind == BuildNodeKind::Executable ||
             node.kind == BuildNodeKind::Library;
    }

    static const BuildTask *find_target_task(
        const BuildGraph &graph,
        const std::string &target)
    {
      const BuildTask *match = nullptr;
      std::size_t matches = 0;

      for (const auto &kv : graph.tasks())
      {
        const BuildTask &task = kv.second;

        if (!is_real_graph_target_task(task))
          continue;

        for (const std::string &outputId : task.outputs)
        {
          const BuildNode *node = graph.find_node(outputId);
          if (!node)
            continue;

          if (!node_is_real_graph_output(*node))
            continue;

          if (!same_target_name(node->path, target))
            continue;

          match = &task;
          ++matches;
        }
      }

      if (matches != 1)
        return nullptr;

      return match;
    }

    static void collect_task_closure(
        const BuildGraph &graph,
        const std::string &taskId,
        const std::unordered_map<std::string, std::string> &outputToTask,
        std::unordered_set<std::string> &selected)
    {
      if (taskId.empty())
        return;

      if (!selected.insert(taskId).second)
        return;

      const BuildTask *task = graph.find_task(taskId);
      if (!task)
        return;

      for (const std::string &inputId : task->inputs)
      {
        const auto producerIt = outputToTask.find(inputId);
        if (producerIt == outputToTask.end())
          continue;

        collect_task_closure(
            graph,
            producerIt->second,
            outputToTask,
            selected);
      }

      for (const std::string &depTaskId : task->deps)
      {
        collect_task_closure(
            graph,
            depTaskId,
            outputToTask,
            selected);
      }
    }

    static std::vector<BuildTask> selected_compile_tasks(
        const BuildGraph &graph,
        const std::unordered_set<std::string> &selected)
    {
      std::vector<BuildTask> out;

      for (const std::string &taskId : selected)
      {
        const BuildTask *task = graph.find_task(taskId);
        if (!task)
          continue;

        if (task->kind == BuildTaskKind::Compile)
          out.push_back(*task);
      }

      std::sort(
          out.begin(),
          out.end(),
          [](const BuildTask &a, const BuildTask &b)
          {
            return a.id < b.id;
          });

      return out;
    }

    static std::vector<BuildTask> dirty_tasks_only(
        const BuildGraph &graph,
        const std::vector<BuildTask> &tasks)
    {
      std::vector<BuildTask> out;

      for (const BuildTask &task : tasks)
      {
        if (graph.task_is_dirty(task))
          out.push_back(task);
      }

      return out;
    }

    static bool graph_has_dirty_project_inputs(const BuildGraph &graph)
    {
      for (const auto &kv : graph.nodes())
      {
        const BuildNode &node = kv.second;

        if (!(node.dirty() || node.missing()))
          continue;

        if (node.kind == BuildNodeKind::Source ||
            node.kind == BuildNodeKind::Header ||
            node.kind == BuildNodeKind::Config)
        {
          return true;
        }
      }

      return false;
    }

    static bool collect_compile_task_paths(
        const BuildGraph &graph,
        const BuildTask &task,
        fs::path &sourcePath,
        fs::path &objectPath,
        std::vector<fs::path> &dependencyPaths)
    {
      sourcePath.clear();
      objectPath.clear();
      dependencyPaths.clear();

      for (const std::string &inputId : task.inputs)
      {
        const BuildNode *node = graph.find_node(inputId);
        if (!node)
          continue;

        if (node->kind == BuildNodeKind::Source && sourcePath.empty())
        {
          sourcePath = node->path;
          continue;
        }

        if (node->kind == BuildNodeKind::Header ||
            node->kind == BuildNodeKind::Config)
        {
          dependencyPaths.push_back(node->path);
        }
      }

      for (const std::string &outputId : task.outputs)
      {
        const BuildNode *node = graph.find_node(outputId);
        if (!node)
          continue;

        if (node->kind == BuildNodeKind::Object)
        {
          objectPath = node->path;
          break;
        }
      }

      return !sourcePath.empty() && !objectPath.empty();
    }

    static BuildTaskResult default_compile_executor(BuildTask &task)
    {
      BuildTaskResult result;
      result.taskId = task.id;

      if (task.command.empty())
      {
        result.state = BuildTaskState::Failed;
        result.exitCode = 127;
        result.output = "Empty build command for task: " + task.id + "\n";
        return result;
      }

      process::Command command;
      command.argv = task.command;
      command.workingDirectory = task.workingDirectory;
      command.mergeStdErr = true;

      const process::Result processResult = process::execute(command);

      result.exitCode = processResult.exitCode;
      result.output = processResult.output;
      if (!processResult.errorMessage.empty())
        result.output += processResult.errorMessage + "\n";

      if (processResult.success())
        result.state = BuildTaskState::Done;
      else
        result.state = BuildTaskState::Failed;

      return result;
    }

    static BuildTaskResult run_cached_compile_task(
        const BuildGraph &graph,
        const ObjectCache &objectCache,
        const BuildGraphExecutorDependencies &dependencies,
        BuildTask &task)
    {
      BuildTaskResult result;
      result.taskId = task.id;

      fs::path sourcePath;
      fs::path objectPath;
      std::vector<fs::path> dependencyPaths;

      if (!collect_compile_task_paths(
              graph,
              task,
              sourcePath,
              objectPath,
              dependencyPaths))
      {
        result.state = BuildTaskState::Failed;
        result.exitCode = 127;
        result.output = "Invalid compile task: " + task.id + "\n";
        return result;
      }

      const fs::path dependencyFilePath =
          dependency_file_for_object(objectPath);

      const ObjectCacheResult restored =
          objectCache.resolve_compile_task(
              task,
              sourcePath,
              dependencyPaths,
              objectPath,
              dependencyFilePath,
              graph.config().buildFingerprint);

      if (restored.hit)
      {
        result.state = BuildTaskState::Skipped;
        result.exitCode = 0;
        result.output = "cache hit: " + sourcePath.string() + "\n";
        return result;
      }

      try
      {
        if (dependencies.executeCompileTask)
          result = dependencies.executeCompileTask(task);
        else
          result = default_compile_executor(task);
      }
      catch (const std::exception &ex)
      {
        result.state = BuildTaskState::Failed;
        result.exitCode = 1;
        result.output = "Compile executor exception for task " +
                        task.id + ": " + ex.what() + "\n";
        return result;
      }
      catch (...)
      {
        result.state = BuildTaskState::Failed;
        result.exitCode = 1;
        result.output = "Compile executor exception for task " +
                        task.id + "\n";
        return result;
      }

      if (result.taskId.empty())
        result.taskId = task.id;

      if (result.exitCode != 0)
        return result;

      const std::string inputHash =
          ObjectCache::compute_input_hash(sourcePath, dependencyPaths);

      const std::string objectKey =
          ObjectCache::compute_object_key(
              sourcePath,
              inputHash,
              task.commandHash,
              graph.config().buildFingerprint);

      (void)objectCache.store(
          objectKey,
          sourcePath,
          objectPath,
          dependencyFilePath,
          inputHash,
          task.commandHash);

      return result;
    }

    static BuildGraphExecutorNinjaRequest make_ninja_target_request(
        const fs::path &buildDir,
        const std::string &target)
    {
      BuildGraphExecutorNinjaRequest request;
      request.buildDir = buildDir;
      request.target = target;
      request.command = {
          "ninja",
          "-C",
          buildDir.string(),
          target};
      return request;
    }

    static BuildGraphExecutorResult run_ninja_target_fallback(
        const BuildGraphExecutorOptions &options,
        const BuildGraphExecutorDependencies &dependencies,
        BuildGraphExecutorStatus status,
        const std::string &reason)
    {
      if (!options.allowNinjaFallback)
      {
        return make_failed_result(
            options,
            status,
            2,
            reason);
      }

      if (!dependencies.executeNinjaTarget)
      {
        return make_failed_result(
            options,
            BuildGraphExecutorStatus::NinjaFailed,
            127,
            "Ninja fallback requested but no Ninja executor was provided.");
      }

      BuildGraphExecutorResult result;
      result.target = options.target;
      result.status = BuildGraphExecutorStatus::DelegatedToNinja;
      result.reason = reason;
      result.usedNinja = true;
      result.usedFallback = true;

      if (!reason.empty())
        result.output = reason + "\n";

      emit_event(
          dependencies,
          BuildGraphExecutorEventKind::DelegatingToNinja,
          reason,
          options.target);

      const BuildGraphExecutorNinjaRequest request =
          make_ninja_target_request(options.buildDir, options.target);

      BuildGraphExecutorNinjaResult ninjaResult;

      try
      {
        ninjaResult = dependencies.executeNinjaTarget(request);
      }
      catch (const std::exception &ex)
      {
        ninjaResult.started = false;
        ninjaResult.exitCode = 127;
        ninjaResult.errorMessage = ex.what();
      }
      catch (...)
      {
        ninjaResult.started = false;
        ninjaResult.exitCode = 127;
        ninjaResult.errorMessage = "Ninja executor exception";
      }

      result.exitCode = ninjaResult.exitCode;

      if (ninjaResult.producedOutput && !ninjaResult.output.empty())
        result.output += ninjaResult.output;

      if (!ninjaResult.errorMessage.empty())
        result.output += ninjaResult.errorMessage + "\n";

      if (ninjaResult.success())
      {
        result.ok = true;
        return result;
      }

      result.ok = false;
      result.status = BuildGraphExecutorStatus::NinjaFailed;

      if (!ninjaResult.displayCommand.empty())
        result.output += ninjaResult.displayCommand + "\n";

      return result;
    }
  } // namespace

  const char *to_string(BuildGraphExecutorStatus status)
  {
    switch (status)
    {
    case BuildGraphExecutorStatus::Success:
      return "success";
    case BuildGraphExecutorStatus::UpToDate:
      return "up-to-date";
    case BuildGraphExecutorStatus::DelegatedToNinja:
      return "delegated-to-ninja";
    case BuildGraphExecutorStatus::InvalidRequest:
      return "invalid-request";
    case BuildGraphExecutorStatus::InvalidGraph:
      return "invalid-graph";
    case BuildGraphExecutorStatus::UnsupportedTarget:
      return "unsupported-target";
    case BuildGraphExecutorStatus::CompileFailed:
      return "compile-failed";
    case BuildGraphExecutorStatus::NinjaFailed:
      return "ninja-failed";
    case BuildGraphExecutorStatus::CacheFailed:
      return "cache-failed";
    default:
      return "unknown";
    }
  }

  bool BuildGraphExecutorNinjaResult::success() const
  {
    return started && exitCode == 0;
  }

  bool BuildGraphExecutorResult::success() const
  {
    return ok && exitCode == 0;
  }

  BuildGraphExecutor::BuildGraphExecutor(BuildGraphExecutorOptions options)
      : options_(std::move(options))
  {
  }

  BuildGraphExecutor::BuildGraphExecutor(
      BuildGraphExecutorOptions options,
      BuildGraphExecutorDependencies dependencies)
      : options_(std::move(options)),
        dependencies_(std::move(dependencies))
  {
  }

  const BuildGraphExecutorOptions &BuildGraphExecutor::options() const
  {
    return options_;
  }

  BuildGraphExecutorResult BuildGraphExecutor::run_target(BuildGraph &graph) const
  {
    BuildGraphExecutorResult result;
    result.target = options_.target;

    emit_event(
        dependencies_,
        BuildGraphExecutorEventKind::ResolvingTarget,
        "graph: starting target executor for `" + options_.target + "`",
        options_.target);

    if (options_.target.empty())
    {
      result.ok = false;
      result.status = BuildGraphExecutorStatus::InvalidRequest;
      result.exitCode = 2;
      result.reason = "Missing graph build target.";
      result.output = "Missing graph build target.\n";
      return result;
    }

    if (graph.empty())
    {
      result.ok = false;
      result.status = BuildGraphExecutorStatus::InvalidGraph;
      result.exitCode = 2;
      result.reason = "Build graph is empty.";
      result.output = "Build graph is empty.\n";
      return result;
    }

    const BuildTask *targetTask =
        find_target_task(graph, options_.target);

    if (!targetTask)
    {
      return run_ninja_target_fallback(
          options_,
          dependencies_,
          BuildGraphExecutorStatus::UnsupportedTarget,
          "Unable to resolve a unique graph output target: " + options_.target);
    }

    result.usedGraph = true;

    emit_event(
        dependencies_,
        BuildGraphExecutorEventKind::TargetResolved,
        "graph: target task resolved: " + targetTask->id,
        options_.target,
        targetTask->id);

    const auto outputToTask = make_output_to_task_map(graph);

    std::unordered_set<std::string> selected;
    collect_task_closure(
        graph,
        targetTask->id,
        outputToTask,
        selected);

    result.selectedTasks = selected.size();

    emit_event(
        dependencies_,
        BuildGraphExecutorEventKind::SelectingTasks,
        "graph: selected " + std::to_string(result.selectedTasks) + " tasks",
        options_.target);

    const std::vector<BuildTask> compileTasks =
        selected_compile_tasks(graph, selected);

    const std::vector<BuildTask> dirtyCompileTasks =
        dirty_tasks_only(graph, compileTasks);

    result.selectedCompileTasks = compileTasks.size();
    result.dirtyCompileTasks = dirtyCompileTasks.size();
    bool delegatedToNinjaBecauseDirtyProjectInput = false;

    if (options_.maxGraphDirtyCompileTasks > 0 &&
        result.dirtyCompileTasks > options_.maxGraphDirtyCompileTasks)
    {
      return run_ninja_target_fallback(
          options_,
          dependencies_,
          BuildGraphExecutorStatus::DelegatedToNinja,
          "Graph target has too many dirty compile tasks: " +
              std::to_string(result.dirtyCompileTasks) +
              " dirty tasks from " +
              std::to_string(result.selectedCompileTasks) +
              " selected compile tasks. Delegating to Ninja.");
    }

    if (!dirtyCompileTasks.empty())
    {
      ObjectCache objectCache =
          dependencies_.objectCacheRoot
              ? ObjectCache(options_.buildDir, *dependencies_.objectCacheRoot)
              : ObjectCache(options_.buildDir);

      if (!objectCache.ensure_layout())
      {
        result.ok = false;
        result.status = BuildGraphExecutorStatus::CacheFailed;
        result.exitCode = 1;
        result.reason = "Unable to initialize object cache.";
        result.output = "Unable to initialize object cache.\n";
        return result;
      }

      BuildSchedulerOptions schedulerOptions;
      schedulerOptions.jobs = options_.jobs;
      schedulerOptions.quiet = true;
      schedulerOptions.stopOnFirstFailure = true;

      BuildScheduler scheduler(schedulerOptions);
      scheduler.add_tasks(dirtyCompileTasks);

      const BuildSchedulerResult compileResult =
          scheduler.run(
              [&](BuildTask &task)
              {
                emit_event(
                    dependencies_,
                    BuildGraphExecutorEventKind::CompilingTask,
                    "graph: compile " + task.id,
                    options_.target,
                    task.id);

                BuildTaskResult taskResult =
                    run_cached_compile_task(
                        graph,
                        objectCache,
                        dependencies_,
                        task);

                if (taskResult.state == BuildTaskState::Skipped)
                {
                  emit_event(
                      dependencies_,
                      BuildGraphExecutorEventKind::CacheHit,
                      taskResult.output,
                      options_.target,
                      task.id);
                }

                return taskResult;
              });

      result.executedCompileTasks = compileResult.done;
      result.skippedCompileTasks = compileResult.skipped;

      for (const BuildTaskResult &taskResult : compileResult.results)
      {
        if (!taskResult.output.empty())
          result.output += taskResult.output;
      }

      if (!compileResult.success())
      {
        result.ok = false;
        result.status = BuildGraphExecutorStatus::CompileFailed;
        result.exitCode = 1;
        result.reason = "One or more compile tasks failed.";
        return result;
      }
    }

    if (dirtyCompileTasks.empty())
    {
      const bool hasDirtyProjectInputs =
          graph_has_dirty_project_inputs(graph);

      bool outputsExist = true;

      for (const std::string &outputId : targetTask->outputs)
      {
        const BuildNode *node = graph.find_node(outputId);

        if (!node || node->missing())
        {
          outputsExist = false;
          break;
        }
      }

      if (outputsExist && !hasDirtyProjectInputs)
      {
        result.ok = true;
        result.status = BuildGraphExecutorStatus::UpToDate;
        result.exitCode = 0;
        result.executedCompileTasks = 0;
        result.skippedCompileTasks = result.selectedCompileTasks;
        result.output += "Graph target is up to date.\n";

        emit_event(
            dependencies_,
            BuildGraphExecutorEventKind::Completed,
            "graph: target outputs exist, skipping ninja",
            options_.target);

        return result;
      }

      if (hasDirtyProjectInputs)
      {
        delegatedToNinjaBecauseDirtyProjectInput = true;
      }
    }

    if (!dependencies_.executeNinjaTarget)
    {
      return make_failed_result(
          options_,
          BuildGraphExecutorStatus::NinjaFailed,
          127,
          "Ninja target execution requested but no Ninja executor was provided.");
    }

    emit_event(
        dependencies_,
        BuildGraphExecutorEventKind::RunningNinja,
        "graph: running ninja target `" + options_.target + "`",
        options_.target);

    const BuildGraphExecutorNinjaRequest request =
        make_ninja_target_request(options_.buildDir, options_.target);

    BuildGraphExecutorNinjaResult ninjaResult;

    try
    {
      ninjaResult = dependencies_.executeNinjaTarget(request);
    }
    catch (const std::exception &ex)
    {
      ninjaResult.started = false;
      ninjaResult.exitCode = 127;
      ninjaResult.errorMessage = ex.what();
    }
    catch (...)
    {
      ninjaResult.started = false;
      ninjaResult.exitCode = 127;
      ninjaResult.errorMessage = "Ninja executor exception";
    }

    result.usedNinja = true;
    result.exitCode = ninjaResult.exitCode;
    result.ok = ninjaResult.success();

    if (result.ok)
    {
      result.status = BuildGraphExecutorStatus::Success;
    }

    if (result.ok && delegatedToNinjaBecauseDirtyProjectInput)
    {
      result.dirtyCompileTasks = 1;
    }

    if (ninjaResult.producedOutput && !ninjaResult.output.empty())
      result.output += ninjaResult.output;

    if (!ninjaResult.errorMessage.empty())
      result.output += ninjaResult.errorMessage + "\n";

    if (!result.ok)
    {
      result.status = BuildGraphExecutorStatus::NinjaFailed;
      result.reason = "Ninja target failed: " + options_.target;
      result.output += "Ninja target failed: " + options_.target + "\n";
      result.output += ninjaResult.displayCommand + "\n";
    }

    emit_event(
        dependencies_,
        BuildGraphExecutorEventKind::Completed,
        "graph: ninja target finished with exit code " +
            std::to_string(result.exitCode),
        options_.target);

    return result;
  }

} // namespace vix::engine
