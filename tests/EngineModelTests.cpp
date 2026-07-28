#include <vix/engine.hpp>

#include <cassert>
#include <string>
#include <vector>

int main()
{
  using namespace vix::engine;

  BuildNode node;
  assert(!node.valid());
  node.id = make_build_node_id(BuildNodeKind::Source, "src/main.cpp");
  node.kind = BuildNodeKind::Source;
  node.add_dependency("header:include/app.hpp");
  node.add_dependency("header:include/app.hpp");
  assert(node.valid());
  assert(node.dirty());
  assert(node.deps.size() == 1);
  assert(to_string(node.kind) == "source");
  assert(build_node_state_from_string("unknown") == BuildNodeState::Dirty);

  const std::vector<std::string> command{"c++", "-c", "src/main.cpp", "-o", "main.o"};
  BuildTask task = make_compile_task(node.id, "object:main.o", command, ".");
  assert(task.valid());
  assert(task.pending());
  assert(task.has_input(node.id));
  assert(task.has_output("object:main.o"));
  assert(!task.commandHash.empty());
  task.mark_running();
  assert(task.running());
  task.mark_done();
  assert(task.done());

  Engine engine;
  Options options;
  ExecutionResult missing = engine.execute(options);
  assert(!missing.success());
  assert(missing.status == ExecutionStatus::Unsupported);
  assert(!missing.diagnostics.empty());

  options.projectDir = ".";
  ExecutionResult deferred = engine.execute(options);
  assert(deferred.status == ExecutionStatus::Unsupported);
  assert(!deferred.events.empty());

  return 0;
}
