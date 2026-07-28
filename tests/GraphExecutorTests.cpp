/**
 *
 *  @file GraphExecutorTests.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Build graph executor tests
 *
 */

#include <vix/engine.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
  namespace fs = std::filesystem;

  fs::path make_temp_dir()
  {
    const auto base = fs::temp_directory_path();
    const auto name = "vix_engine_graph_executor_tests_" +
                      std::to_string(
                          std::chrono::steady_clock::now()
                              .time_since_epoch()
                              .count());
    const auto dir = base / name;
    fs::create_directories(dir);
    return dir;
  }

  void write_file(const fs::path &path, const std::string &text)
  {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
  }

  vix::engine::BuildNode node(
      vix::engine::BuildNodeKind kind,
      const fs::path &path,
      vix::engine::BuildNodeState state)
  {
    vix::engine::BuildNode out;
    out.id = vix::engine::make_build_node_id(kind, path);
    out.kind = kind;
    out.state = state;
    out.path = path;
    out.hash = path.generic_string();
    return out;
  }

  struct Fixture
  {
    fs::path root;
    fs::path project;
    fs::path build;
    fs::path cacheRoot;
    fs::path source;
    fs::path header;
    fs::path object;
    fs::path depFile;
    fs::path executable;

    vix::engine::BuildGraph graph;
  };

  Fixture make_fixture(
      vix::engine::BuildNodeState sourceState =
          vix::engine::BuildNodeState::Dirty,
      vix::engine::BuildNodeState headerState =
          vix::engine::BuildNodeState::Clean,
      vix::engine::BuildNodeState objectState =
          vix::engine::BuildNodeState::Missing,
      vix::engine::BuildNodeState executableState =
          vix::engine::BuildNodeState::Missing)
  {
    Fixture fixture;
    fixture.root = make_temp_dir();
    fixture.project = fixture.root / "project";
    fixture.build = fixture.root / "build";
    fixture.cacheRoot = fixture.root / "cache";
    fixture.source = fixture.project / "src" / "main.cpp";
    fixture.header = fixture.project / "include" / "app.hpp";
    fixture.object = fixture.build / "CMakeFiles" / "app.dir" / "main.cpp.o";
    fixture.depFile = vix::engine::dependency_file_for_object(fixture.object);
    fixture.executable = fixture.build / "app";

    write_file(fixture.source, "#include \"app.hpp\"\nint main(){return value();}\n");
    write_file(fixture.header, "inline int value(){return 1;}\n");
    fs::create_directories(fixture.build);

    if (objectState != vix::engine::BuildNodeState::Missing)
      write_file(fixture.object, "old object");

    if (executableState != vix::engine::BuildNodeState::Missing)
      write_file(fixture.executable, "exe");

    vix::engine::BuildGraphConfig config;
    config.projectDir = fixture.project;
    config.buildDir = fixture.build;
    config.objectDir = fixture.build / ".vix" / "obj";
    config.compiler = "c++";
    config.buildFingerprint = "fingerprint";

    fixture.graph = vix::engine::BuildGraph(config);

    const auto sourceNode =
        node(vix::engine::BuildNodeKind::Source, fixture.source, sourceState);
    const auto headerNode =
        node(vix::engine::BuildNodeKind::Header, fixture.header, headerState);
    const auto objectNode =
        node(vix::engine::BuildNodeKind::Object, fixture.object, objectState);
    const auto executableNode =
        node(vix::engine::BuildNodeKind::Executable, fixture.executable, executableState);

    assert(fixture.graph.add_node(sourceNode));
    assert(fixture.graph.add_node(headerNode));
    assert(fixture.graph.add_node(objectNode));
    assert(fixture.graph.add_node(executableNode));

    vix::engine::BuildTask compile;
    compile.id = "compile:main";
    compile.kind = vix::engine::BuildTaskKind::Compile;
    compile.state = vix::engine::BuildTaskState::Pending;
    compile.inputs = {sourceNode.id, headerNode.id};
    compile.outputs = {objectNode.id};
    compile.command = {"compile", fixture.object.string(), fixture.depFile.string()};
    compile.commandHash = "command-hash";
    compile.workingDirectory = fixture.project;
    assert(fixture.graph.add_task(compile));

    vix::engine::BuildTask link;
    link.id = "link:app";
    link.kind = vix::engine::BuildTaskKind::Link;
    link.state = vix::engine::BuildTaskState::Pending;
    link.inputs = {objectNode.id};
    link.outputs = {executableNode.id};
    link.command = {"link"};
    link.commandHash = "link-hash";
    assert(fixture.graph.add_task(link));

    return fixture;
  }

  vix::engine::BuildGraphExecutorOptions options_for(const Fixture &fixture)
  {
    vix::engine::BuildGraphExecutorOptions options;
    options.buildDir = fixture.build;
    options.target = "app";
    options.jobs = 1;
    return options;
  }

  vix::engine::BuildGraphExecutorDependencies dependencies_for(
      const Fixture &fixture,
      int *compileCalls = nullptr,
      int *ninjaCalls = nullptr,
      std::vector<vix::engine::BuildGraphExecutorEvent> *events = nullptr)
  {
    vix::engine::BuildGraphExecutorDependencies dependencies;
    dependencies.objectCacheRoot = fixture.cacheRoot;
    dependencies.executeCompileTask =
        [compileCalls](vix::engine::BuildTask &task)
        {
          if (compileCalls)
            ++*compileCalls;

          if (task.command.size() > 1)
            write_file(task.command[1], "compiled object");
          if (task.command.size() > 2)
            write_file(task.command[2], task.command[1] + ": source header\n");

          vix::engine::BuildTaskResult result;
          result.taskId = task.id;
          result.state = vix::engine::BuildTaskState::Done;
          result.exitCode = 0;
          result.output = "compiled " + task.id + "\n";
          return result;
        };
    dependencies.executeNinjaTarget =
        [ninjaCalls](const vix::engine::BuildGraphExecutorNinjaRequest &request)
        {
          if (ninjaCalls)
            ++*ninjaCalls;

          assert(request.target == "app" || request.target == "missing" ||
                 request.target == "generated" || request.target == "ambiguous");
          assert(!request.buildDir.empty());
          assert(request.command.size() == 4);
          assert(request.command[0] == "ninja");
          assert(request.command[1] == "-C");
          assert(request.command[2] == request.buildDir.string());
          assert(request.command[3] == request.target);

          vix::engine::BuildGraphExecutorNinjaResult result;
          result.started = true;
          result.exitCode = 0;
          result.producedOutput = true;
          result.output = "ninja ok\n";
          result.displayCommand = "ninja -C " + request.buildDir.string() + " " + request.target;
          return result;
        };
    dependencies.onEvent =
        [events](const vix::engine::BuildGraphExecutorEvent &event)
        {
          if (events)
            events->push_back(event);
        };
    return dependencies;
  }
}

int main()
{
  using namespace vix::engine;

  {
    auto fixture = make_fixture();
    auto options = options_for(fixture);
    options.target.clear();
    BuildGraphExecutor executor(options, dependencies_for(fixture));
    const auto result = executor.run_target(fixture.graph);
    assert(!result.ok);
    assert(result.status == BuildGraphExecutorStatus::InvalidRequest);
    assert(result.exitCode == 2);
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    BuildGraph empty(fixture.graph.config());
    BuildGraphExecutor executor(options_for(fixture), dependencies_for(fixture));
    const auto result = executor.run_target(empty);
    assert(!result.ok);
    assert(result.status == BuildGraphExecutorStatus::InvalidGraph);
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture(
        BuildNodeState::Clean,
        BuildNodeState::Clean,
        BuildNodeState::Clean,
        BuildNodeState::Clean);
    int ninjaCalls = 0;
    BuildGraphExecutor executor(
        options_for(fixture),
        dependencies_for(fixture, nullptr, &ninjaCalls));
    const auto result = executor.run_target(fixture.graph);
    assert(result.ok);
    assert(result.status == BuildGraphExecutorStatus::UpToDate);
    assert(result.selectedTasks == 2);
    assert(result.selectedCompileTasks == 1);
    assert(result.skippedCompileTasks == 1);
    assert(ninjaCalls == 0);
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    int compileCalls = 0;
    int ninjaCalls = 0;
    std::vector<BuildGraphExecutorEvent> events;
    BuildGraphExecutor executor(
        options_for(fixture),
        dependencies_for(fixture, &compileCalls, &ninjaCalls, &events));
    const auto result = executor.run_target(fixture.graph);
    assert(result.ok);
    assert(result.status == BuildGraphExecutorStatus::Success);
    assert(result.executedCompileTasks == 1);
    assert(result.skippedCompileTasks == 0);
    assert(result.usedGraph);
    assert(result.usedNinja);
    assert(compileCalls == 1);
    assert(ninjaCalls == 1);
    assert(!events.empty());
    assert(fs::exists(fixture.object));
    assert(fs::exists(fixture.depFile));
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    ObjectCache cache(fixture.build, fixture.cacheRoot);
    assert(cache.ensure_layout());
    const std::vector<fs::path> deps = {fixture.header};
    const std::string inputHash =
        ObjectCache::compute_input_hash(fixture.source, deps);
    const std::string key =
        ObjectCache::compute_object_key(
            fixture.source,
            inputHash,
            "command-hash",
            fixture.graph.config().buildFingerprint);
    write_file(fixture.root / "cached.o", "cached object");
    write_file(fixture.root / "cached.d", "cached deps");
    assert(cache.store(
        key,
        fixture.source,
        fixture.root / "cached.o",
        fixture.root / "cached.d",
        inputHash,
        "command-hash"));

    int compileCalls = 0;
    int ninjaCalls = 0;
    BuildGraphExecutor executor(
        options_for(fixture),
        dependencies_for(fixture, &compileCalls, &ninjaCalls));
    const auto result = executor.run_target(fixture.graph);
    assert(result.ok);
    assert(result.executedCompileTasks == 0);
    assert(result.skippedCompileTasks == 1);
    assert(compileCalls == 0);
    assert(ninjaCalls == 1);
    assert(fs::exists(fixture.object));
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    auto dependencies = dependencies_for(fixture);
    dependencies.executeCompileTask =
        [](BuildTask &task)
        {
          BuildTaskResult result;
          result.taskId = task.id;
          result.state = BuildTaskState::Failed;
          result.exitCode = 42;
          result.output = "compile failed\n";
          return result;
        };
    BuildGraphExecutor executor(options_for(fixture), dependencies);
    const auto result = executor.run_target(fixture.graph);
    assert(!result.ok);
    assert(result.status == BuildGraphExecutorStatus::CompileFailed);
    assert(result.usedGraph);
    assert(!result.usedNinja);
    assert(result.output.find("compile failed") != std::string::npos);
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    auto dependencies = dependencies_for(fixture);
    dependencies.executeCompileTask =
        [](BuildTask &)
        {
          throw std::runtime_error("boom");
          return BuildTaskResult{};
        };
    BuildGraphExecutor executor(options_for(fixture), dependencies);
    const auto result = executor.run_target(fixture.graph);
    assert(!result.ok);
    assert(result.status == BuildGraphExecutorStatus::CompileFailed);
    assert(result.output.find("boom") != std::string::npos);
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    auto options = options_for(fixture);
    options.target = "missing";
    int ninjaCalls = 0;
    BuildGraphExecutor executor(
        options,
        dependencies_for(fixture, nullptr, &ninjaCalls));
    const auto result = executor.run_target(fixture.graph);
    assert(result.ok);
    assert(result.status == BuildGraphExecutorStatus::DelegatedToNinja);
    assert(result.usedFallback);
    assert(result.usedNinja);
    assert(ninjaCalls == 1);
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    auto options = options_for(fixture);
    options.target = "missing";
    options.allowNinjaFallback = false;
    BuildGraphExecutor executor(options, dependencies_for(fixture));
    const auto result = executor.run_target(fixture.graph);
    assert(!result.ok);
    assert(result.status == BuildGraphExecutorStatus::UnsupportedTarget);
    assert(!result.usedNinja);
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    auto options = options_for(fixture);
    options.maxGraphDirtyCompileTasks = 0;
    auto dependencies = dependencies_for(fixture);
    dependencies.executeNinjaTarget = {};
    BuildGraphExecutor executor(options, dependencies);
    const auto result = executor.run_target(fixture.graph);
    assert(!result.ok);
    assert(result.status == BuildGraphExecutorStatus::NinjaFailed);
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    auto dependencies = dependencies_for(fixture);
    dependencies.executeNinjaTarget =
        [](const BuildGraphExecutorNinjaRequest &)
        {
          BuildGraphExecutorNinjaResult result;
          result.started = true;
          result.exitCode = 9;
          result.displayCommand = "ninja app";
          result.output = "ninja failed\n";
          result.producedOutput = true;
          return result;
        };
    BuildGraphExecutor executor(options_for(fixture), dependencies);
    const auto result = executor.run_target(fixture.graph);
    assert(!result.ok);
    assert(result.status == BuildGraphExecutorStatus::NinjaFailed);
    assert(result.output.find("ninja failed") != std::string::npos);
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    auto options = options_for(fixture);
    options.maxGraphDirtyCompileTasks = 0;

    auto generated = node(
        BuildNodeKind::Executable,
        fixture.build / "generated",
        BuildNodeState::Missing);
    assert(fixture.graph.add_node(generated));
    BuildTask task;
    task.id = "generate:target";
    task.kind = BuildTaskKind::Generate;
    task.outputs = {generated.id};
    assert(fixture.graph.add_task(task));

    options.target = "generated";
    int ninjaCalls = 0;
    BuildGraphExecutor executor(
        options,
        dependencies_for(fixture, nullptr, &ninjaCalls));
    const auto result = executor.run_target(fixture.graph);
    assert(result.ok);
    assert(result.status == BuildGraphExecutorStatus::DelegatedToNinja);
    assert(ninjaCalls == 1);
    fs::remove_all(fixture.root);
  }

  {
    auto fixture = make_fixture();
    auto duplicate = node(
        BuildNodeKind::Executable,
        fixture.root / "other" / "ambiguous",
        BuildNodeState::Missing);
    assert(fixture.graph.add_node(duplicate));
    BuildTask first;
    first.id = "link:first";
    first.kind = BuildTaskKind::Link;
    first.outputs = {duplicate.id};
    assert(fixture.graph.add_task(first));

    auto secondNode = node(
        BuildNodeKind::Executable,
        fixture.root / "third" / "ambiguous",
        BuildNodeState::Missing);
    assert(fixture.graph.add_node(secondNode));
    BuildTask second;
    second.id = "link:second";
    second.kind = BuildTaskKind::Link;
    second.outputs = {secondNode.id};
    assert(fixture.graph.add_task(second));

    auto options = options_for(fixture);
    options.target = "ambiguous";
    int ninjaCalls = 0;
    BuildGraphExecutor executor(
        options,
        dependencies_for(fixture, nullptr, &ninjaCalls));
    const auto result = executor.run_target(fixture.graph);
    assert(result.ok);
    assert(result.status == BuildGraphExecutorStatus::DelegatedToNinja);
    assert(ninjaCalls == 1);
    fs::remove_all(fixture.root);
  }

  assert(std::string(to_string(BuildGraphExecutorStatus::Success)) == "success");
  assert(std::string(to_string(BuildGraphExecutorStatus::UpToDate)) == "up-to-date");
  BuildGraphExecutorNinjaResult ninjaSuccess;
  ninjaSuccess.started = true;
  ninjaSuccess.exitCode = 0;
  assert(ninjaSuccess.success());

  return 0;
}
