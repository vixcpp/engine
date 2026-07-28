#include <vix/engine.hpp>

#include <cassert>
#include <string>

int main()
{
  using namespace vix::engine;

  const std::string deps =
      "build/main.o: src/main.cpp include/app.hpp include/config\\ value.hpp \\\n"
      " include/next.hpp\n"
      "include/app.hpp:\n";

  auto parsedDeps = parse_dependency_file_text(deps, "build/main.d");
  assert(parsedDeps.has_value());
  assert(parsedDeps->target == normalize_dependency_path("build/main.o"));
  assert(parsedDeps->has_dependency("src/main.cpp"));
  assert(parsedDeps->has_dependency("include/config value.hpp"));
  assert(parsedDeps->has_dependency("include/next.hpp"));
  assert(dependency_file_for_object("build/main.o") == std::filesystem::path("build/main.d"));

  const std::string compileDb = R"json([
    {
      "directory": "/tmp/project",
      "command": "c++ -I include -DNAME='hello world' -c src/main.cpp -o CMakeFiles/app.dir/src/main.cpp.o",
      "file": "src/main.cpp"
    },
    {
      "directory": "/tmp/project",
      "arguments": ["c++", "-c", "src/lib.cpp", "-o", "lib.o"],
      "file": "src/lib.cpp",
      "output": "lib.o"
    }
  ])json";

  auto commands = parse_compile_commands_text(compileDb, "/tmp/project/build/compile_commands.json");
  assert(commands.has_value());
  assert(commands->size() == 2);
  assert(commands->at(0).valid());
  assert(commands->at(0).has_output());
  assert(commands->at(0).arguments.at(3) == "-DNAME=hello world");
  assert(commands->at(1).output == std::filesystem::path("/tmp/project/lib.o"));

  auto tokens = split_compile_command("c++ \"two words.cpp\" -DNAME=\\\"value\\\"");
  assert(tokens.size() == 3);
  assert(tokens.at(1) == "two words.cpp");
  assert(tokens.at(2) == "-DNAME=\"value\"");

  return 0;
}
