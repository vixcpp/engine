#include <vix/engine.hpp>

#include <cassert>
#include <chrono>
#include <fstream>
#include <string>

namespace
{
  std::filesystem::path make_temp_dir()
  {
    const auto base = std::filesystem::temp_directory_path();
    const auto name = "vix_engine_graph_tests_" +
                      std::to_string(
                          std::chrono::steady_clock::now()
                              .time_since_epoch()
                              .count());
    const auto dir = base / name;
    std::filesystem::create_directories(dir);
    return dir;
  }

  void write_file(const std::filesystem::path &path, const std::string &text)
  {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
  }
}

int main()
{
  using namespace vix::engine;
  namespace fs = std::filesystem;

  const fs::path root = make_temp_dir();
  const fs::path project = root / "project";
  const fs::path build = root / "build";
  const fs::path objectDir = build / ".vix" / "obj";
  fs::create_directories(objectDir);

  BuildGraphConfig invalid;
  assert(!invalid.valid());

  BuildGraphConfig config;
  config.projectDir = project;
  config.buildDir = build;
  config.objectDir = objectDir;
  config.compiler = "c++";
  config.includeDirs.push_back((project / "include").string());
  config.flags.push_back("-Wall");
  assert(config.valid());

  write_file(project / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\n");
  write_file(project / "src/main.cpp", "#include \"app.hpp\"\nint main(){return value();}\n");
  write_file(project / "include/app.hpp", "inline int value(){return 1;}\n");
  write_file(project / "build-scratch/ignored.cpp", "int ignored;\n");

  BuildGraph graph(config);
  assert(graph.empty());

  BuildNode manualNode = make_file_build_node(BuildNodeKind::Config, project / "manual.cfg");
  manualNode.id = "manual";
  assert(graph.add_node(manualNode));
  assert(graph.find_node("manual") != nullptr);

  BuildTask manualTask;
  manualTask.id = "manual-task";
  assert(graph.add_task(manualTask));
  assert(graph.find_task("manual-task") != nullptr);
  graph.clear();
  assert(graph.empty());

  const BuildGraphScanResult scan = graph.scan_project();
  assert(scan.sources == 1);
  assert(scan.headers == 1);
  assert(scan.configs == 1);
  assert(graph.sorted_node_ids() == graph.sorted_node_ids());
  for (const auto &id : graph.sorted_node_ids())
    assert(id.find("ignored.cpp") == std::string::npos);

  const fs::path object = build / "CMakeFiles/app.dir/src/main.cpp.o";
  write_file(object, "object");

  const fs::path compileDb = build / "compile_commands.json";
  write_file(
      compileDb,
      "[{\"directory\":\"" + project.string() +
          "\",\"arguments\":[\"c++\",\"-c\",\"src/main.cpp\",\"-o\",\"" +
          object.string() +
          "\"],\"file\":\"src/main.cpp\",\"output\":\"" +
          object.string() + "\"}]\n");

  assert(graph.load_compile_commands(compileDb) == 1);
  assert(graph.compile_tasks().size() == 1);

  const fs::path depFile = dependency_file_for_object(object);
  write_file(depFile, object.string() + ": " + (project / "src/main.cpp").string() +
                          " " + (project / "include/app.hpp").string() + "\n");
  graph.load_dependency_files();

  const fs::path ninjaFile = build / "build.ninja";
  write_file(
      ninjaFile,
      "rule CXX_EXECUTABLE_LINKER\n"
      "  command = c++ $in -o $out\n"
      "build app: CXX_EXECUTABLE_LINKER CMakeFiles/app.dir/src/main.cpp.o\n");

  assert(graph.load_ninja_build(ninjaFile) == 1);
  assert(graph.tasks().size() == 2);

  graph.mark_all_dirty();
  assert(!graph.dirty_compile_tasks().empty());

  BuildGraph previous = graph;
  graph.mark_clean_from_previous(previous);
  graph.propagate_dirty();
  assert(graph.dirty_compile_tasks().empty());

  for (const auto &kv : graph.tasks())
    assert(!kv.first.empty());

  const auto taskIds = graph.sorted_task_ids();
  assert(!taskIds.empty());
  assert(taskIds == graph.sorted_task_ids());
  const auto compileTasks = graph.compile_tasks();
  assert(!compileTasks.empty());
  assert(!graph.task_is_dirty(compileTasks.front()));

  const std::string fingerprint1 = graph.fingerprint();
  const std::string fingerprint2 = graph.fingerprint();
  assert(!fingerprint1.empty());
  assert(fingerprint1 == fingerprint2);

  const fs::path graphPath = BuildGraph::default_graph_path(build);
  assert(graphPath == build / ".vix" / "build-graph.vix");
  assert(graph.save(graphPath));
  auto loaded = BuildGraph::load(graphPath);
  assert(loaded.has_value());
  assert(loaded->config().projectDir == project);
  assert(loaded->nodes().size() == graph.nodes().size());
  assert(loaded->tasks().size() == graph.tasks().size());

  const fs::path compatPath = root / "compat-graph.vix";
  write_file(
      compatPath,
      "vix-build-graph\n"
      "project=" +
          project.string() + "\n"
                             "build=" +
          build.string() + "\n"
                           "object=" +
          objectDir.string() + "\n"
                               "compiler=c++\n"
                               "fingerprint=compat\n"
                               "node|source:" +
          (project / "src/main.cpp").string() +
          "|source|clean|" + (project / "src/main.cpp").string() +
          "|abcdef|1|2|header:" + (project / "include/app.hpp").string() + "\n"
                                                                           "task|compile:compat|compile|pending|source:" +
          (project / "src/main.cpp").string() +
          "|object:" + object.string() +
          "||c++;-c;src/main.cpp;-o;" + object.string() +
          "|1234|" + project.string() + "|" + depFile.string() + "|0\n");

  auto compat = BuildGraph::load(compatPath);
  assert(compat.has_value());
  assert(compat->config().buildFingerprint == "compat");
  assert(compat->nodes().size() == 1);
  assert(compat->tasks().size() == 1);

  std::error_code ec;
  fs::remove_all(root, ec);
  return 0;
}
