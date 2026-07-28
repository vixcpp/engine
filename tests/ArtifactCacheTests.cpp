/**
 *
 *  @file ArtifactCacheTests.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Artifact cache and build-state tests
 *
 */

#include <vix/engine.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace
{
  namespace fs = std::filesystem;
  using namespace vix::engine;

  void require(bool condition, const char *message)
  {
    if (!condition)
      throw std::runtime_error(message);
  }

  struct TempDir
  {
    fs::path path;

    TempDir()
    {
      path = fs::temp_directory_path() /
             ("vix-engine-artifact-cache-test-" +
              std::to_string(
                  std::chrono::steady_clock::now()
                      .time_since_epoch()
                      .count()));
      fs::create_directories(path);
    }

    ~TempDir()
    {
      std::error_code ec;
      fs::remove_all(path, ec);
    }
  };

  void write_file(const fs::path &path, const std::string &text)
  {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
  }

  Artifact make_artifact()
  {
    Artifact artifact;
    artifact.package = "my/app";
    artifact.version = "local version";
    artifact.target = "x86_64/linux";
    artifact.compiler = "clang++ 18";
    artifact.buildType = "Debug";
    artifact.fingerprint = "abc/def";
    return artifact;
  }

  ArtifactCachePaths make_paths(const TempDir &temp)
  {
    ArtifactCachePaths paths;
    paths.buildCacheRoot = temp.path / "home" / ".vix" / "cache" / "build";
    return paths;
  }

  ProjectInput input(
      std::string path,
      std::uint64_t size,
      std::uint64_t mtime,
      std::uint64_t hash)
  {
    ProjectInput out;
    out.path = std::move(path);
    out.size = size;
    out.mtime = mtime;
    out.hash = hash;
    return out;
  }
}

int main()
{
  {
    TempDir temp;
    const ArtifactCachePaths paths = make_paths(temp);
    const Artifact artifact = make_artifact();

    require(
        ArtifactCache::cache_root().generic_string().find(".vix/cache/build") !=
            std::string::npos,
        "default cache root structure");
    require(
        ArtifactCache::index_path(paths) == paths.buildCacheRoot / "index.vix",
        "explicit index path");

    const fs::path artifactPath = ArtifactCache::artifact_path(paths, artifact);
    require(
        artifactPath ==
                paths.buildCacheRoot /
                "x86_64_linux" /
                "clang++_18" /
                "Debug" /
                "my_app_local_version" /
                "abc_def",
        "sanitized artifact path");
    require(
        ArtifactCache::artifact_key(artifact) ==
            "x86_64_linux|clang++_18|Debug|my_app|local_version|abc_def",
        "sanitized artifact key");

    Artifact unsafe = artifact;
    unsafe.package = "../../pkg";
    unsafe.target = "..";
    const fs::path unsafePath = ArtifactCache::artifact_path(paths, unsafe);
    for (const auto &part : unsafePath)
      require(part != "..", "unsafe path component sanitized");
  }

  {
    TempDir temp;
    const ArtifactCachePaths paths = make_paths(temp);
    Artifact artifact = make_artifact();

    require(!ArtifactCache::exists(paths, artifact), "missing artifact");
    require(ArtifactCache::ensure_layout(paths, artifact), "layout creation");

    require(!ArtifactCache::resolve(paths, artifact), "resolve after layout still requires manifest");
    require(!ArtifactCache::exists(paths, artifact), "layout without manifest incomplete");

    require(ArtifactCache::write_manifest(paths, artifact), "manifest write");
    require(ArtifactCache::exists(paths, artifact), "exists with manifest");

    Artifact materialized = *ArtifactCache::resolve(paths, artifact);
    require(materialized.root == ArtifactCache::artifact_path(paths, artifact), "resolved root");
    require(fs::is_directory(materialized.include), "include dir");
    require(fs::is_directory(materialized.lib), "lib dir");
    require(fs::is_directory(materialized.root / "share"), "share dir");

    const auto manifest = ArtifactCache::read_manifest(materialized.root);
    require(manifest.has_value(), "manifest read");
    require(manifest->package == artifact.package, "manifest package");
    require(manifest->version == artifact.version, "manifest version");
    require(manifest->buildType == artifact.buildType, "manifest build type");

    write_file(materialized.root / "manifest.json", "{not-json");
    require(!ArtifactCache::read_manifest(materialized.root), "malformed manifest");
  }

  {
    TempDir temp;
    const ArtifactCachePaths paths = make_paths(temp);
    Artifact artifact = make_artifact();
    require(ArtifactCache::write_manifest(paths, artifact), "manifest writes index");

    const auto entry = ArtifactCache::find_index_entry(paths, artifact);
    require(entry.has_value(), "index lookup");
    require(entry->key == ArtifactCache::artifact_key(artifact), "index key");
    require(entry->root == ArtifactCache::artifact_path(paths, artifact).string(), "index root");

    std::ofstream out(ArtifactCache::index_path(paths), std::ios::app);
    out << "malformed\n";
    out << "key=" << ArtifactCache::artifact_key(artifact)
        << "\tpackage=stale\tversion=local\ttarget=x\tcompiler=y\tbuildType=z\tfingerprint=f\troot="
        << (temp.path / "missing").string() << "\tupdatedUnixMs=999\n";
    out.close();

    const auto afterMalformed = ArtifactCache::find_index_entry(paths, artifact);
    require(afterMalformed.has_value(), "malformed and stale entries ignored");
    require(afterMalformed->package == artifact.package, "last valid matching entry");
  }

  {
    TempDir temp;
    const fs::path project = temp.path / "project";
    write_file(project / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\n");
    write_file(project / "src" / "main.cpp", "int main(){return 0;}\n");
    write_file(project / "include" / "app.hpp", "#pragma once\n");
    write_file(project / "cmake" / "toolchain.cmake", "set(X 1)\n");
    write_file(project / "vix.lock", "lock\n");
    write_file(project / "build" / "ignored.cpp", "ignored\n");
    write_file(project / ".git" / "ignored.hpp", "ignored\n");
    write_file(project / "node_modules" / "ignored.hpp", "ignored\n");

    const auto inputs = ArtifactCache::snapshot_project_inputs(project);
    std::vector<std::string> paths;
    for (const auto &item : inputs)
      paths.push_back(item.path);

    require(!inputs.empty(), "inputs discovered");
    require(std::is_sorted(paths.begin(), paths.end()), "inputs sorted");
    require(std::find(paths.begin(), paths.end(), "CMakeLists.txt") != paths.end(), "cmake tracked");
    require(std::find(paths.begin(), paths.end(), "src/main.cpp") != paths.end(), "source tracked");
    require(std::find(paths.begin(), paths.end(), "include/app.hpp") != paths.end(), "header tracked");
    require(std::find(paths.begin(), paths.end(), "cmake/toolchain.cmake") != paths.end(), "cmake dir tracked");
    require(std::find(paths.begin(), paths.end(), "vix.lock") != paths.end(), "lock tracked");
    require(std::find(paths.begin(), paths.end(), "build/ignored.cpp") == paths.end(), "build ignored");
    require(std::find(paths.begin(), paths.end(), ".git/ignored.hpp") == paths.end(), "git ignored");
    require(std::find(paths.begin(), paths.end(), "node_modules/ignored.hpp") == paths.end(), "node_modules ignored");

    std::vector<ProjectInput> previous = inputs;
    previous.front().hash = 999999;
    const auto reused = ArtifactCache::snapshot_project_inputs(project, &previous);
    require(!reused.empty(), "reused snapshot");
    require(reused.front().hash == 999999, "previous hash reused when metadata unchanged");

    write_file(project / "src" / "main.cpp", "int main(){return 1;}\n");
    const auto changed = ArtifactCache::snapshot_project_inputs(project, &previous);
    require(
        ArtifactCache::compute_inputs_fingerprint(changed) !=
            ArtifactCache::compute_inputs_fingerprint(previous),
        "changed input changes fingerprint");
  }

  {
    TempDir temp;
    const fs::path file = temp.path / "abc.txt";
    write_file(file, "abc");

    require(ArtifactCache::hash_string("hello") == 25347132070217633ull, "hash string fixed");
    require(ArtifactCache::hash_file_content(file) == 16242233503745875709ull, "file hash fixed");

    std::vector<ProjectInput> inputs = {
        input("src/main.cpp", 12, 34, 56),
        input("include/app.hpp", 78, 90, 123)};
    require(
        ArtifactCache::compute_inputs_fingerprint(inputs) == "d9da38ea029db7f1",
        "input fingerprint fixed");
    require(
        ArtifactCache::compute_inputs_fingerprint({}) == "14650fb0739d0383",
        "empty fingerprint fixed");
  }

  {
    TempDir temp;
    const fs::path build = temp.path / "build";
    const fs::path artifactRoot = temp.path / "artifact";
    const fs::path binary = build / "app";
    write_file(binary, "exe");
    fs::create_directories(artifactRoot);
#ifndef _WIN32
    std::error_code ec;
    fs::permissions(
        binary,
        fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write,
        fs::perm_options::add,
        ec);
#endif

    const std::vector<ProjectInput> inputs = {
        input("src/main.cpp", 12, 34, 56)};
    BuildState state = ArtifactCache::make_build_state(
        "sig",
        "project",
        artifactRoot.string(),
        binary.string(),
        "app",
        "dev",
        "Debug",
        "native",
        "clang",
        inputs);

    require(state.schemaVersion == 2, "state schema");
    require(!state.inputsFingerprint.empty(), "state fingerprint");
    require(state.createdUnixMs != 0, "state created timestamp");
    require(ArtifactCache::write_build_state(build, state), "state write");
    require(ArtifactCache::build_state_path(build) == build / ".vix-build-state", "state path");

    const auto loaded = ArtifactCache::read_build_state(build);
    require(loaded.has_value(), "state read");
    require(loaded->signature == "sig", "state signature");
    require(loaded->inputs.size() == 1, "state inputs");

    require(
        ArtifactCache::build_state_matches(
            *loaded,
            "sig",
            "project",
            "app",
            "dev",
            "Debug",
            "native",
            "clang",
            inputs),
        "state matches");

    require(
        !ArtifactCache::build_state_matches(*loaded, "bad", "project", "app", "dev", "Debug", "native", "clang", inputs),
        "signature mismatch");
    require(
        !ArtifactCache::build_state_matches(*loaded, "sig", "bad", "app", "dev", "Debug", "native", "clang", inputs),
        "project mismatch");
    require(
        !ArtifactCache::build_state_matches(*loaded, "sig", "project", "other", "dev", "Debug", "native", "clang", inputs),
        "target mismatch");
    require(
        !ArtifactCache::build_state_matches(*loaded, "sig", "project", "app", "release", "Debug", "native", "clang", inputs),
        "preset mismatch");
    require(
        !ArtifactCache::build_state_matches(*loaded, "sig", "project", "app", "dev", "Release", "native", "clang", inputs),
        "build type mismatch");
    require(
        !ArtifactCache::build_state_matches(*loaded, "sig", "project", "app", "dev", "Debug", "other", "clang", inputs),
        "triple mismatch");
    require(
        !ArtifactCache::build_state_matches(*loaded, "sig", "project", "app", "dev", "Debug", "native", "gcc", inputs),
        "compiler mismatch");

    auto changedInputs = inputs;
    changedInputs.front().hash = 999;
    require(
        !ArtifactCache::build_state_matches(
            *loaded,
            "sig",
            "project",
            "app",
            "dev",
            "Debug",
            "native",
            "clang",
            changedInputs),
        "input mismatch");

    write_file(build / ".vix-build-state", "bad\n");
    require(!ArtifactCache::read_build_state(build), "malformed state rejected");
  }

  {
    TempDir temp;
    const fs::path build = temp.path / "build";
    write_file(
        build / ".vix-build-state",
        "vix-build-state-v2\n"
        "schemaVersion=2\n"
        "signature=sig\n"
        "projectFingerprint=project\n"
        "inputsFingerprint=d9da38ea029db7f1\n"
        "artifactRoot=/tmp/artifact\n"
        "lastBinary=/tmp/app\n"
        "buildTarget=app\n"
        "preset=dev\n"
        "buildType=Debug\n"
        "target=native\n"
        "compiler=clang\n"
        "createdUnixMs=1\n"
        "updatedUnixMs=2\n"
        "inputsCount=1\n"
        "input=src/main.cpp\t12\t34\t56\n");
    const auto state = ArtifactCache::read_build_state(build);
    require(state.has_value(), "current-format state fixture");
    require(state->inputs.size() == 1, "fixture input");
    require(state->inputs.front().path == "src/main.cpp", "fixture input path");
  }

  return 0;
}
