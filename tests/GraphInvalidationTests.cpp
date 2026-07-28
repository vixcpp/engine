#include <vix/engine/BuildGraph.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
namespace engine = vix::engine;
namespace watch = vix::engine::watch;

namespace
{
  struct TempDir
  {
    fs::path path;

    TempDir()
    {
      path = fs::temp_directory_path() /
             ("vix-engine-graph-invalidation-tests-" +
              std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count()));
      fs::create_directories(path);
    }

    ~TempDir()
    {
      std::error_code ec;
      fs::remove_all(path, ec);
    }
  };

  static void write_file(const fs::path &path, const std::string &content)
  {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
  }

  static engine::BuildGraph make_graph(const fs::path &root)
  {
    engine::BuildGraphConfig config;
    config.projectDir = root;
    config.buildDir = root / "build-ninja";
    config.objectDir = config.buildDir / ".vix" / "obj";
    config.compiler = "c++";

    engine::BuildGraph graph(config);

    const fs::path source = root / "src" / "main.cpp";
    const fs::path header = root / "include" / "app.hpp";
    const fs::path object = config.objectDir / "main.o";

    write_file(source, "#include \"app.hpp\"\nint main(){return value();}\n");
    write_file(header, "#pragma once\ninline int value(){return 0;}\n");

    engine::BuildNode sourceNode =
        engine::make_file_build_node(engine::BuildNodeKind::Source, source);
    engine::BuildNode headerNode =
        engine::make_file_build_node(engine::BuildNodeKind::Header, header);
    engine::BuildNode objectNode =
        engine::make_file_build_node(engine::BuildNodeKind::Object, object);

    sourceNode.mark_clean();
    headerNode.mark_clean();
    objectNode.mark_clean();

    sourceNode.hash = "source-old";
    headerNode.hash = "header-old";
    objectNode.hash = "object-old";

    objectNode.add_dependency(sourceNode.id);
    objectNode.add_dependency(headerNode.id);

    graph.add_node(sourceNode);
    graph.add_node(headerNode);
    graph.add_node(objectNode);

    engine::BuildTask task =
        engine::make_compile_task(
            sourceNode.id,
            objectNode.id,
            {"c++", "-c", source.string(), "-o", object.string()},
            root);
    task.id = "compile:main";
    task.add_input(headerNode.id);
    task.state = engine::BuildTaskState::Done;
    graph.add_task(task);

    return graph;
  }

  static void known_source_marks_dirty_task()
  {
    TempDir tmp;
    engine::BuildGraph graph = make_graph(tmp.path);

    const fs::path source = tmp.path / "src" / "main.cpp";
    write_file(source, "int main(){return 1;}\n");

    watch::Event event;
    event.kind = watch::EventKind::Modified;
    event.path = source;

    const auto result = graph.invalidate_paths({event});
    assert(result.relevant);
    assert(!result.structuralChange);
    assert(result.changedNodes == 1);
    assert(result.affectedTasks == 1);
    assert(result.dirtyTaskIds.size() == 1);
    assert(result.dirtyTaskIds[0] == "compile:main");

    const engine::BuildTask *task = graph.find_task("compile:main");
    assert(task);
    assert(task->state == engine::BuildTaskState::Pending);
  }

  static void known_header_propagates_to_compile_task()
  {
    TempDir tmp;
    engine::BuildGraph graph = make_graph(tmp.path);

    const fs::path header = tmp.path / "include" / "app.hpp";
    write_file(header, "#pragma once\ninline int value(){return 2;}\n");

    watch::Event event;
    event.kind = watch::EventKind::Modified;
    event.path = header;

    const auto result = graph.invalidate_paths({event});
    assert(result.relevant);
    assert(!result.structuralChange);
    assert(result.affectedTasks == 1);
  }

  static void removed_source_is_structural_and_dirty()
  {
    TempDir tmp;
    engine::BuildGraph graph = make_graph(tmp.path);

    const fs::path source = tmp.path / "src" / "main.cpp";
    fs::remove(source);

    watch::Event event;
    event.kind = watch::EventKind::Removed;
    event.path = source;

    const auto result = graph.invalidate_paths({event});
    assert(result.relevant);
    assert(result.changedNodes == 1);
    assert(result.affectedTasks == 1);
  }

  static void unknown_source_is_structural()
  {
    TempDir tmp;
    engine::BuildGraph graph = make_graph(tmp.path);

    const fs::path source = tmp.path / "src" / "extra.cpp";
    write_file(source, "int extra(){return 0;}\n");

    watch::Event event;
    event.kind = watch::EventKind::Added;
    event.path = source;

    const auto result = graph.invalidate_paths({event});
    assert(result.relevant);
    assert(result.structuralChange);
    assert(!result.unknownPaths.empty());
  }

  static void unknown_unrelated_file_is_ignored()
  {
    TempDir tmp;
    engine::BuildGraph graph = make_graph(tmp.path);

    const fs::path file = tmp.path / "README.md";
    write_file(file, "docs\n");

    watch::Event event;
    event.kind = watch::EventKind::Modified;
    event.path = file;

    const auto result = graph.invalidate_paths({event});
    assert(!result.relevant);
    assert(!result.structuralChange);
    assert(result.affectedTasks == 0);
  }

  static void unchanged_known_source_event_is_ignored()
  {
    TempDir tmp;
    engine::BuildGraph graph = make_graph(tmp.path);

    const fs::path source = tmp.path / "src" / "main.cpp";

    watch::Event event;
    event.kind = watch::EventKind::Modified;
    event.path = source;

    const auto first = graph.invalidate_paths({event});
    assert(first.relevant);
    assert(first.changedNodes == 1);

    const std::string sourceId =
        engine::make_build_node_id(engine::BuildNodeKind::Source, source);
    const std::string headerId =
        engine::make_build_node_id(
            engine::BuildNodeKind::Header,
            tmp.path / "include" / "app.hpp");
    const std::string objectId =
        engine::make_build_node_id(
            engine::BuildNodeKind::Object,
            tmp.path / "build-ninja" / ".vix" / "obj" / "main.o");

    if (engine::BuildNode *node = graph.find_node(sourceId))
      node->mark_clean();
    if (engine::BuildNode *node = graph.find_node(headerId))
      node->mark_clean();
    if (engine::BuildNode *node = graph.find_node(objectId))
      node->mark_clean();
    if (engine::BuildTask *task = graph.find_task("compile:main"))
      task->state = engine::BuildTaskState::Done;

    const auto second = graph.invalidate_paths({event});
    assert(!second.relevant);
    assert(!second.structuralChange);
    assert(second.changedNodes == 0);
    assert(second.affectedTasks == 0);

    const engine::BuildTask *task = graph.find_task("compile:main");
    assert(task);
    assert(task->state == engine::BuildTaskState::Done);
  }
}

int main()
{
  known_source_marks_dirty_task();
  known_header_propagates_to_compile_task();
  removed_source_is_structural_and_dirty();
  unknown_source_is_structural();
  unknown_unrelated_file_is_ignored();
  unchanged_known_source_event_is_ignored();

  std::cout << "GraphInvalidationTests passed\n";
  return 0;
}
