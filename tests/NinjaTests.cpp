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
    const auto name = "vix_engine_ninja_tests_" +
                      std::to_string(
                          std::chrono::steady_clock::now()
                              .time_since_epoch()
                              .count());
    const auto dir = base / name;
    std::filesystem::create_directories(dir);
    return dir;
  }
}

int main()
{
  using namespace vix::engine;
  namespace fs = std::filesystem;

  assert(!parse_build_ninja_text("").has_value());
  assert(default_build_ninja_path("build") == fs::path("build/build.ninja"));

  const fs::path dir = make_temp_dir();
  const fs::path ninjaPath = dir / "build.ninja";

  const std::string text =
      "builddir = build\n"
      "cxx = c++\n"
      "rule CXX_COMPILER\n"
      "  command = $cxx -c $in -o $out\n"
      "  description = compile $out\n"
      "rule CXX_STATIC_LIBRARY_LINKER\n"
      "  command = ar qc $out $in\n"
      "rule CXX_EXECUTABLE_LINKER\n"
      "  command = c++ $in -o $out\n"
      "rule CMAKE_SYMLINK_LIBRARY\n"
      "  command = cmake -E copy $in $out\n"
      "rule install\n"
      "  command = cmake --install .\n"
      "rule phony\n"
      "  command = :\n"
      "build CMakeFiles/app.dir/src/main.cpp.o: CXX_COMPILER src/main.cpp | include/app.hpp || cmake_object_order_depends_target_app\n"
      "  depfile = CMakeFiles/app.dir/src/main.cpp.o.d\n"
      "build libapp.a: CXX_STATIC_LIBRARY_LINKER CMakeFiles/app.dir/src/main.cpp.o\n"
      "build app: CXX_EXECUTABLE_LINKER CMakeFiles/app.dir/src/main.cpp.o libapp.a\n"
      "build copied$ file.txt: CMAKE_SYMLINK_LIBRARY source$ file.txt\n"
      "build install/strip: install app\n"
      "build utility: phony app\n"
      "build continued.o: CXX_COMPILER src/continued.cpp $\n"
      " include/continued.hpp\n";

  auto parsed = parse_build_ninja_text(text, ninjaPath);
  assert(parsed.has_value());
  assert(parsed->valid());
  assert(parsed->variables.at("builddir") == "build");
  assert(parsed->rules.count("CXX_COMPILER") == 1);
  assert(parsed->edges.size() == 7);

  const NinjaEdge &compile = parsed->edges.at(0);
  assert(compile.kind == NinjaEdgeKind::Compile);
  assert(compile.explicitInputs.size() == 1);
  assert(compile.implicitInputs.size() == 1);
  assert(compile.orderOnlyInputs.size() == 1);
  assert(compile.variables.at("depfile") == "CMakeFiles/app.dir/src/main.cpp.o.d");
  assert(to_string(compile.kind) == "compile");

  assert(parsed->edges.at(1).kind == NinjaEdgeKind::Archive);
  assert(parsed->edges.at(2).kind == NinjaEdgeKind::Link);
  assert(parsed->edges.at(3).kind == NinjaEdgeKind::Copy);
  assert(parsed->edges.at(3).primary_output().filename() == fs::path("copied file.txt"));
  assert(parsed->edges.at(4).kind == NinjaEdgeKind::Install);
  assert(parsed->edges.at(5).kind == NinjaEdgeKind::Utility);
  assert(parsed->edges.at(6).explicitInputs.size() == 2);

  NinjaEdge manual;
  manual.rule = "custom_link";
  manual.outputs.push_back(dir / "manual");
  NinjaRule rule;
  rule.name = "custom_link";
  rule.variables["command"] = "c++ input.o -o manual";
  assert(classify_ninja_edge(manual, &rule) == NinjaEdgeKind::Link);

  {
    std::ofstream out(ninjaPath);
    out << text;
  }

  auto fromDisk = read_build_ninja(ninjaPath);
  assert(fromDisk.has_value());
  assert(fromDisk->edges.size() == parsed->edges.size());
  assert(resolve_ninja_path(dir, "relative/path") == (dir / "relative/path").lexically_normal());

  std::error_code ec;
  fs::remove_all(dir, ec);
  return 0;
}
