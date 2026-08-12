#include <vix/engine/BuildScheduler.hpp>
#include <vix/engine/BuildParallelism.hpp>

#include <atomic>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vix::engine;

namespace
{
  static BuildTask task(std::string id, std::vector<std::string> deps = {})
  {
    BuildTask t;
    t.id = std::move(id);
    t.kind = BuildTaskKind::Compile;
    t.deps = std::move(deps);
    t.command = {"echo", t.id};
    return t;
  }

  static BuildTaskResult done_result(const BuildTask &task)
  {
    BuildTaskResult result;
    result.taskId = task.id;
    result.state = BuildTaskState::Done;
    result.exitCode = 0;
    result.output = "done:" + task.id;
    return result;
  }

  static void require(bool condition, const std::string &message)
  {
    if (!condition)
      throw std::runtime_error(message);
  }

  static void test_empty_scheduler()
  {
    BuildScheduler scheduler;
    const BuildSchedulerResult result =
        scheduler.run(
            [](BuildTask &t)
            {
              return done_result(t);
            });

    require(result.total == 0, "empty scheduler total");
    require(result.success(), "empty scheduler success");
  }

  static void test_insertion_lookup_replacement_and_clear()
  {
    BuildScheduler scheduler;
    BuildTask invalid;

    require(!scheduler.add_task(invalid), "invalid task rejected");
    require(scheduler.add_task(task("b")), "valid task inserted");
    require(scheduler.add_task(task("a")), "second task inserted");
    require(scheduler.find_task("a") != nullptr, "find task");
    require(scheduler.find_task("missing") == nullptr, "missing task lookup");

    BuildTask replacement = task("a");
    replacement.kind = BuildTaskKind::Link;
    require(scheduler.add_task(replacement), "replacement inserted");
    require(scheduler.find_task("a")->kind == BuildTaskKind::Link, "task replaced by id");

    const std::vector<std::string> ids = scheduler.sorted_task_ids();
    require(ids.size() == 2 && ids[0] == "a" && ids[1] == "b", "deterministic sorted ids");

    scheduler.clear();
    require(scheduler.tasks().empty(), "scheduler clear");
    require(scheduler.add_task(task("after-clear")), "reuse after clear");
  }

  static void test_add_multiple_and_missing_dependencies()
  {
    BuildScheduler scheduler;
    scheduler.add_tasks({task("a"), task("b", {"missing", "a"})});

    require(scheduler.has_missing_dependencies(), "missing dependency detected");
    const std::vector<std::string> missing = scheduler.missing_dependencies();
    require(missing.size() == 1 && missing[0] == "missing", "missing dependency ids");

    const BuildSchedulerResult result =
        scheduler.run(
            [](BuildTask &t)
            {
              return done_result(t);
            });

    require(!result.success(), "missing dependency run fails");
    require(result.failed == 2, "missing dependency fails all tasks");
  }

  static void test_dependency_completion_and_order()
  {
    BuildScheduler scheduler(BuildSchedulerOptions{1, true, true});
    scheduler.add_tasks({task("compile"), task("link", {"compile"})});

    require(!scheduler.dependencies_complete(*scheduler.find_task("link")), "dependency initially incomplete");

    std::vector<std::string> order;
    const BuildSchedulerResult result =
        scheduler.run(
            [&](BuildTask &t)
            {
              order.push_back(t.id);
              return done_result(t);
            });

    require(result.success(), "dependency ordered run succeeds");
    require(order.size() == 2 && order[0] == "compile" && order[1] == "link", "dependency execution order");
    require(scheduler.dependencies_complete(*scheduler.find_task("link")), "dependency complete after run");
  }

  static void test_single_worker_and_aggregation()
  {
    BuildScheduler scheduler(BuildSchedulerOptions{1, true, false});
    scheduler.add_tasks({task("a"), task("b")});

    const BuildSchedulerResult result =
        scheduler.run(
            [](BuildTask &t)
            {
              BuildTaskResult result = done_result(t);
              result.output = "output:" + t.id;
              return result;
            });

    require(result.success(), "successful aggregation");
    require(result.total == 2, "aggregation total");
    require(result.done == 2, "aggregation done");
    require(result.results.size() == 2, "aggregation results");
    require(result.results[0].exitCode == 0, "exit code propagation");
    require(!result.results[0].output.empty(), "output propagation");
  }

  static void test_failure_and_stop_on_first_failure()
  {
    BuildScheduler scheduler(BuildSchedulerOptions{1, true, true});
    scheduler.add_tasks({task("a"), task("b")});

    const BuildSchedulerResult result =
        scheduler.run(
            [](BuildTask &t)
            {
              BuildTaskResult result;
              result.taskId = t.id;
              result.state = t.id == "a" ? BuildTaskState::Failed : BuildTaskState::Done;
              result.exitCode = t.id == "a" ? 42 : 0;
              result.output = "ran:" + t.id;
              return result;
            });

    require(!result.success(), "failed aggregation");
    require(result.failed >= 1, "failed count");
    require(result.results.size() == 1, "stop on first failure stops new tasks");
    require(result.results[0].exitCode == 42, "failure exit code propagated");
  }

  static void test_failed_dependency_blocks_dependents()
  {
    BuildScheduler scheduler(BuildSchedulerOptions{1, true, false});
    scheduler.add_tasks({task("a"), task("b", {"a"})});

    const BuildSchedulerResult result =
        scheduler.run(
            [](BuildTask &t)
            {
              BuildTaskResult result;
              result.taskId = t.id;
              result.state = BuildTaskState::Failed;
              result.exitCode = 1;
              return result;
            });

    require(!result.success(), "failed dependency run fails");
    require(result.failed == 2, "dependent task marked failed when blocked");
    require(scheduler.find_task("b")->failed(), "dependent task failed");
  }

  static void test_executor_exception()
  {
    BuildScheduler scheduler(BuildSchedulerOptions{1, true, true});
    scheduler.add_task(task("throws"));

    const BuildSchedulerResult result =
        scheduler.run(
            [](BuildTask &) -> BuildTaskResult
            {
              throw std::runtime_error("executor failure");
            });

    require(!result.success(), "exception result fails");
    require(result.failed == 1, "exception failed count");
    require(result.results.size() == 1, "exception result recorded");
    require(result.results[0].exitCode == 127, "exception exit code");
    require(result.results[0].output.find("executor failure") != std::string::npos, "exception output");
  }

  static void test_blocked_and_cycle_behavior()
  {
    BuildScheduler blocked;
    BuildTask readyButNotQueued = task("ready");
    readyButNotQueued.mark_ready();
    blocked.add_task(readyButNotQueued);

    const BuildSchedulerResult blockedResult =
        blocked.run(
            [](BuildTask &t)
            {
              return done_result(t);
            });

    require(!blockedResult.success(), "blocked graph fails");

    BuildScheduler cycle;
    cycle.add_tasks({task("a", {"b"}), task("b", {"a"})});

    const BuildSchedulerResult cycleResult =
        cycle.run(
            [](BuildTask &t)
            {
              return done_result(t);
            });

    require(!cycleResult.success(), "cycle graph fails");
    require(cycleResult.failed == 2, "cycle marks both tasks failed");
  }

  static void test_automatic_jobs()
  {
    require(BuildScheduler::default_jobs() >= 1, "default jobs positive");
    require(recommended_build_jobs(1) == 1, "one thread keeps one build job");
    require(recommended_build_jobs(2) == 1, "two threads reserve one for the system");
    require(recommended_build_jobs(4) == 3, "four threads reserve interactive capacity");
    require(recommended_build_jobs(8) == 6, "eight threads do not saturate the machine");

    BuildScheduler scheduler(BuildSchedulerOptions{0, true, true});
    scheduler.add_task(task("a"));

    const BuildSchedulerResult result =
        scheduler.run(
            [](BuildTask &t)
            {
              return done_result(t);
            });

    require(result.success(), "automatic jobs run succeeds");
  }

  static void test_parallel_independent_tasks_can_overlap()
  {
    BuildScheduler scheduler(BuildSchedulerOptions{2, true, true});
    scheduler.add_tasks({task("a"), task("b")});

    std::mutex mutex;
    std::condition_variable cv;
    int entered = 0;
    bool release = false;

    const BuildSchedulerResult result =
        scheduler.run(
            [&](BuildTask &t)
            {
              {
                std::unique_lock<std::mutex> lock(mutex);
                ++entered;
                cv.notify_all();
                cv.wait(
                    lock,
                    [&]()
                    {
                      return release || entered >= 2;
                    });
                release = true;
                cv.notify_all();
              }

              return done_result(t);
            });

    require(result.success(), "parallel run succeeds");
    require(entered == 2, "independent tasks overlapped");
  }
} // namespace

int main()
{
  try
  {
    test_empty_scheduler();
    test_insertion_lookup_replacement_and_clear();
    test_add_multiple_and_missing_dependencies();
    test_dependency_completion_and_order();
    test_single_worker_and_aggregation();
    test_failure_and_stop_on_first_failure();
    test_failed_dependency_blocks_dependents();
    test_executor_exception();
    test_blocked_and_cycle_behavior();
    test_automatic_jobs();
    test_parallel_independent_tasks_can_overlap();
  }
  catch (const std::exception &ex)
  {
    std::cerr << "SchedulerTests failed: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}
