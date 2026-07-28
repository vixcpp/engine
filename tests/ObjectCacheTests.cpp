/**
 *
 *  @file ObjectCacheTests.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Object cache tests
 *
 */

#include <vix/engine/ObjectCache.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace vix::engine;

namespace
{
  namespace fs = std::filesystem;

  struct TempDir
  {
    fs::path path;

    TempDir()
    {
      path = fs::temp_directory_path() /
             ("vix-engine-object-cache-test-" + std::to_string(std::rand()));
      fs::remove_all(path);
      fs::create_directories(path);
    }

    ~TempDir()
    {
      std::error_code ec;
      fs::remove_all(path, ec);
    }
  };

  static void require(bool condition, const std::string &message)
  {
    if (!condition)
      throw std::runtime_error(message);
  }

  static void write_file(const fs::path &path, const std::string &content)
  {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
  }

  static std::string read_file(const fs::path &path)
  {
    std::ifstream in(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
  }

  static ObjectCache make_cache(const TempDir &temp)
  {
    return ObjectCache(temp.path / "build", temp.path / "cache" / "objects");
  }

  static BuildTask compile_task(std::string commandHash = "command-hash")
  {
    BuildTask task;
    task.id = "compile:main";
    task.kind = BuildTaskKind::Compile;
    task.commandHash = std::move(commandHash);
    return task;
  }

  static void test_construction_and_paths()
  {
    TempDir temp;
    const ObjectCache cache = make_cache(temp);

    require(cache.root() == temp.path / "cache" / "objects", "explicit root");
    require(cache.index_path() == cache.root() / "index.vix", "index path");
    require(cache.object_path_for_key("abc") == cache.root() / "abc" / "object.o", "object path");
    require(cache.dependency_path_for_key("abc") == cache.root() / "abc" / "object.d", "dependency path");
    require(cache.ensure_layout(), "layout creation");
    require(fs::is_directory(cache.root()), "layout exists");

    const ObjectCache defaultCache(temp.path / "build");
    require(defaultCache.root().generic_string().find(".vix/cache/objects") != std::string::npos,
            "default root structure");
  }

  static void test_hashing_regression_and_determinism()
  {
    const std::string inputHash =
        ObjectCache::compute_input_hash(
            "src/main.cpp",
            {"include/z.hpp", "include/a.hpp", "include/a.hpp"});

    require(inputHash == "0027f91ffed20947", "fixed missing-file input hash");

    const std::string key =
        ObjectCache::compute_object_key(
            "src/main.cpp",
            inputHash,
            "command-hash",
            "build-fingerprint");

    require(key == "ce377869273cc8ff", "fixed object key");
    require(ObjectCache::compute_object_key("src/main.cpp", inputHash, "other-command", "build-fingerprint") ==
                "26d580a3a771b9ed",
            "command hash affects key");
    require(ObjectCache::compute_object_key("src/main.cpp", inputHash, "command-hash", "other-build") ==
                "103c27a2c97a3c09",
            "build fingerprint affects key");

    TempDir temp;
    const fs::path source = temp.path / "src" / "main.cpp";
    const fs::path depA = temp.path / "include" / "a.hpp";
    const fs::path depB = temp.path / "include" / "b.hpp";

    write_file(source, "int main() { return 0; }\n");
    write_file(depA, "#pragma once\n");
    write_file(depB, "constexpr int b = 1;\n");

    const std::string first =
        ObjectCache::compute_input_hash(source, {depB, depA, depA});
    const std::string second =
        ObjectCache::compute_input_hash(source, {depA, depB});

    require(first == second, "dependency ordering and duplicates are deterministic");

    write_file(source, "int main() { return 1; }\n");
    require(ObjectCache::compute_input_hash(source, {depA, depB}) != first,
            "source content affects hash");

    write_file(source, "int main() { return 0; }\n");
    write_file(depA, "#pragma once\n#define A 1\n");
    require(ObjectCache::compute_input_hash(source, {depA, depB}) != second,
            "dependency content affects hash");

    require(ObjectCache::compute_input_hash(source, {depA, depB, temp.path / "missing.hpp"}) !=
                ObjectCache::compute_input_hash(source, {depA, depB}),
            "missing dependency affects hash");
  }

  static void test_store_read_and_manifest()
  {
    TempDir temp;
    const ObjectCache cache = make_cache(temp);

    const fs::path source = temp.path / "src" / "main.cpp";
    const fs::path object = temp.path / "build" / "main.o";
    const fs::path dep = temp.path / "build" / "main.d";

    write_file(source, "int main(){}\n");
    write_file(object, "object-bytes");
    write_file(dep, "main.o: main.cpp main.hpp\n");

    require(!cache.store("", source, object, dep, "input", "command"), "empty key store rejected");
    require(!cache.store("missing-object", source, temp.path / "missing.o", dep, "input", "command"),
            "missing object store rejected");

    require(cache.store("key1", source, object, dep, "input", "command"), "store with dep");
    require(cache.exists("key1"), "exists after store");
    require(cache.store("key1", source, object, dep, "input", "command"), "repeated store");

    const auto entry = cache.read_entry("key1");
    require(entry.has_value(), "read stored entry");
    require(entry->key == "key1", "entry key");
    require(entry->source == source.lexically_normal(), "entry source");
    require(entry->object == cache.object_path_for_key("key1").lexically_normal(), "entry object");
    require(entry->dependencyFile == cache.dependency_path_for_key("key1").lexically_normal(), "entry dep");
    require(entry->commandHash == "command", "entry command hash");
    require(entry->inputHash == "input", "entry input hash");
    require(entry->finalHash == "key1", "entry final hash");
    require(entry->objectSize == 12, "entry object size");
    require(entry->updatedUnixMs > 0, "entry timestamp");

    const std::string manifest = read_file(cache.root() / "key1" / "manifest.json");
    require(manifest.find("\"dependency_file\"") != std::string::npos, "manifest dependency field");
    require(manifest.find("\"updated_unix_ms\"") != std::string::npos, "manifest timestamp field");

    write_file(cache.root() / "bad" / "manifest.json", "{not valid json");
    require(!cache.read_entry("bad"), "malformed manifest rejected");

    fs::remove(cache.object_path_for_key("key1"));
    require(!cache.read_entry("key1"), "entry with missing cached object rejected");
  }

  static void test_store_without_dependency_file()
  {
    TempDir temp;
    const ObjectCache cache = make_cache(temp);

    const fs::path source = temp.path / "src" / "main.cpp";
    const fs::path object = temp.path / "build" / "main.o";
    const fs::path dep = temp.path / "build" / "missing.d";

    write_file(source, "int main(){}\n");
    write_file(object, "object");

    require(cache.store("nodep", source, object, dep, "input", "command"), "store without dep");
    require(cache.read_entry("nodep").has_value(), "read entry without dep");
    require(!fs::exists(cache.dependency_path_for_key("nodep")), "missing dep not materialized in cache");
  }

  static void test_current_format_compatibility_fixture()
  {
    TempDir temp;
    const ObjectCache cache = make_cache(temp);
    const fs::path object = cache.object_path_for_key("fixture");
    const fs::path dep = cache.dependency_path_for_key("fixture");

    write_file(object, "fixture-object");
    write_file(dep, "fixture.o: fixture.cpp\n");

    std::ostringstream manifest;
    manifest << "{\n";
    manifest << "  \"key\": \"fixture\",\n";
    manifest << "  \"source\": \"src/fixture.cpp\",\n";
    manifest << "  \"object\": \"" << object.string() << "\",\n";
    manifest << "  \"dependency_file\": \"" << dep.string() << "\",\n";
    manifest << "  \"command_hash\": \"cmd\",\n";
    manifest << "  \"input_hash\": \"input\",\n";
    manifest << "  \"final_hash\": \"fixture\",\n";
    manifest << "  \"object_size\": \"14\",\n";
    manifest << "  \"updated_unix_ms\": \"12345\"\n";
    manifest << "}\n";

    write_file(cache.root() / "fixture" / "manifest.json", manifest.str());

    const auto entry = cache.read_entry("fixture");
    require(entry.has_value(), "current format fixture read");
    require(entry->commandHash == "cmd", "fixture command hash");
    require(entry->inputHash == "input", "fixture input hash");
    require(entry->objectSize == 14, "fixture object size");
    require(entry->updatedUnixMs == 12345, "fixture timestamp");
  }

  static void test_restore()
  {
    TempDir temp;
    const ObjectCache cache = make_cache(temp);

    const fs::path source = temp.path / "src" / "main.cpp";
    const fs::path object = temp.path / "build" / "main.o";
    const fs::path dep = temp.path / "build" / "main.d";

    write_file(source, "int main(){}\n");
    write_file(object, "object-v1");
    write_file(dep, "main.o: main.cpp\n");

    require(cache.store("restore", source, object, dep, "input", "command"), "store for restore");

    const fs::path restoredObject = temp.path / "out" / "nested" / "main.o";
    const fs::path restoredDep = temp.path / "out" / "nested" / "main.d";
    write_file(restoredObject, "old");

    const ObjectCacheResult result = cache.restore("restore", restoredObject, restoredDep);
    require(result.hit, "restore hit");
    require(result.materializedObject == restoredObject.lexically_normal(), "materialized object path");
    require(result.materializedDependencyFile == restoredDep.lexically_normal(), "materialized dep path");
    require(read_file(restoredObject) == "object-v1", "object restored and replaced");
    require(read_file(restoredDep) == "main.o: main.cpp\n", "dep restored");

    require(!cache.restore("missing", restoredObject, restoredDep).hit, "missing key restore miss");

    fs::path badParent = temp.path / "file-parent";
    write_file(badParent, "not a directory");
    require(!cache.restore("restore", badParent / "main.o", temp.path / "x.d").hit,
            "copy failure restore miss");
  }

  static void test_restore_without_dependency_file()
  {
    TempDir temp;
    const ObjectCache cache = make_cache(temp);

    const fs::path source = temp.path / "src" / "main.cpp";
    const fs::path object = temp.path / "build" / "main.o";
    const fs::path dep = temp.path / "build" / "missing.d";

    write_file(source, "int main(){}\n");
    write_file(object, "object");
    require(cache.store("nodep", source, object, dep, "input", "command"), "store no dep for restore");

    const fs::path restoredObject = temp.path / "out" / "main.o";
    const fs::path restoredDep = temp.path / "out" / "main.d";
    const ObjectCacheResult result = cache.restore("nodep", restoredObject, restoredDep);

    require(result.hit, "restore hit without dep");
    require(read_file(restoredObject) == "object", "object restored without dep");
    require(!fs::exists(restoredDep), "missing cached dep not restored");
  }

  static void test_compile_task_resolution()
  {
    TempDir temp;
    const ObjectCache cache = make_cache(temp);

    const fs::path source = temp.path / "src" / "main.cpp";
    const fs::path header = temp.path / "src" / "main.hpp";
    const fs::path object = temp.path / "build" / "main.o";
    const fs::path dep = temp.path / "build" / "main.d";
    const std::string fingerprint = "build-fingerprint";

    write_file(source, "int main(){return answer;}\n");
    write_file(header, "constexpr int answer = 0;\n");
    write_file(object, "compiled-object");
    write_file(dep, "main.o: main.cpp main.hpp\n");

    const BuildTask task = compile_task();
    const std::string inputHash = ObjectCache::compute_input_hash(source, {header});
    const std::string key =
        ObjectCache::compute_object_key(source, inputHash, task.commandHash, fingerprint);

    require(cache.store(key, source, object, dep, inputHash, task.commandHash), "store compile key");

    fs::remove(object);
    fs::remove(dep);

    const ObjectCacheResult hit =
        cache.resolve_compile_task(task, source, {header}, object, dep, fingerprint);
    require(hit.hit, "compile task cache hit");
    require(read_file(object) == "compiled-object", "compile object restored");
    require(read_file(dep) == "main.o: main.cpp main.hpp\n", "compile dep restored");

    require(!cache.resolve_compile_task(task, source, {header}, temp.path / "other.o", temp.path / "other.d", "other")
                 .hit,
            "changed build fingerprint misses");

    write_file(header, "constexpr int answer = 1;\n");
    require(!cache.resolve_compile_task(task, source, {header}, temp.path / "changed.o", temp.path / "changed.d", fingerprint)
                 .hit,
            "changed dependency misses");

    BuildTask invalidKind = task;
    invalidKind.kind = BuildTaskKind::Link;
    require(!cache.resolve_compile_task(invalidKind, source, {header}, object, dep, fingerprint).hit,
            "invalid task kind misses");

    BuildTask missingCommandHash = task;
    missingCommandHash.commandHash.clear();
    require(!cache.resolve_compile_task(missingCommandHash, source, {header}, object, dep, fingerprint).hit,
            "missing command hash misses");
  }
} // namespace

int main()
{
  try
  {
    test_construction_and_paths();
    test_hashing_regression_and_determinism();
    test_store_read_and_manifest();
    test_store_without_dependency_file();
    test_current_format_compatibility_fixture();
    test_restore();
    test_restore_without_dependency_file();
    test_compile_task_resolution();
  }
  catch (const std::exception &ex)
  {
    std::cerr << "ObjectCacheTests failed: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}
