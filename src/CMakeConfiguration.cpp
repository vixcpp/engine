/**
 *
 *  @file BuildTools.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Build graph task model
 *
 */
#include <vix/engine/CMakeConfiguration.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#include "ExecutableLookup.hpp"

namespace vix::engine
{
  namespace
  {
    std::string warning_check_cxx_compiler()
    {
      const char *cxx = std::getenv("CXX");

      if (cxx && *cxx)
        return cxx;

      if (detail::executable_on_path("clang++"))
        return "clang++";

      if (detail::executable_on_path("g++"))
        return "g++";

      return "c++";
    }

    std::optional<std::string> warning_check_c_compiler(
        const std::string &cxxCompiler)
    {
      const char *cc = std::getenv("CC");

      if (cc && *cc)
        return std::string(cc);

      if (cxxCompiler == "clang++" && detail::executable_on_path("clang"))
        return std::string("clang");

      if (cxxCompiler == "g++" && detail::executable_on_path("gcc"))
        return std::string("gcc");

      return std::nullopt;
    }

    bool warning_check_uses_clang(const std::string &compiler)
    {
      return compiler.find("clang") != std::string::npos;
    }

    std::string warning_check_cxx_flags(
        const std::string &compiler)
    {
      std::string flags =
          "-Wall "
          "-Wextra "
          "-Wpedantic "
          "-Wshadow "
          "-Wconversion "
          "-Wsign-conversion "
          "-Wformat=2 "
          "-Wold-style-cast "
          "-Woverloaded-virtual";

      if (warning_check_uses_clang(compiler))
      {
        flags +=
            " -Wlogical-op-parentheses"
            " -Wunreachable-code";
      }

      return flags;
    }
  } // namespace

  std::vector<CMakeVariable> make_cmake_variables(
      const CMakeConfigurationOptions &options)
  {
    std::vector<CMakeVariable> vars;
    vars.reserve(32);

    vars.emplace_back("CMAKE_BUILD_TYPE", options.buildType);
    vars.emplace_back("CMAKE_EXPORT_COMPILE_COMMANDS", "ON");

#ifndef _WIN32
    {
      std::error_code ec;
      if (std::filesystem::exists("/usr/bin/ar", ec) && !ec)
        vars.emplace_back("CMAKE_AR", "/usr/bin/ar");
    }

    {
      std::error_code ec;
      if (std::filesystem::exists("/usr/bin/ranlib", ec) && !ec)
        vars.emplace_back("CMAKE_RANLIB", "/usr/bin/ranlib");
    }
#endif

    if (options.warningCheck)
    {
      const std::string cxxCompiler = warning_check_cxx_compiler();
      const std::optional<std::string> cCompiler =
          warning_check_c_compiler(cxxCompiler);

      vars.emplace_back("VIX_ENABLE_WARNINGS", "ON");
      vars.emplace_back(
          "CMAKE_CXX_FLAGS",
          warning_check_cxx_flags(cxxCompiler));

      if (!std::getenv("CXX"))
        vars.emplace_back("CMAKE_CXX_COMPILER", cxxCompiler);

      if (!std::getenv("CC") && cCompiler)
        vars.emplace_back("CMAKE_C_COMPILER", *cCompiler);
    }

    if (!options.targetTriple.empty())
      vars.emplace_back("CMAKE_TOOLCHAIN_FILE", options.toolchainFile.string());

    if (options.linkStatic)
      vars.emplace_back("VIX_LINK_STATIC", "ON");

    if (!options.targetTriple.empty())
      vars.emplace_back("VIX_TARGET_TRIPLE", options.targetTriple);

    (void)options.globalPackagesFile;

    if (!options.sdkConfigDir.empty())
    {
      vars.emplace_back("Vix_DIR", options.sdkConfigDir.string());
      vars.emplace_back("vix_DIR", options.sdkConfigDir.string());
    }

    if (options.launcher && !options.launcher->empty())
    {
      vars.emplace_back("CMAKE_C_COMPILER_LAUNCHER", *options.launcher);
      vars.emplace_back("CMAKE_CXX_COMPILER_LAUNCHER", *options.launcher);
    }

    if (options.fastLinkerFlag && !options.fastLinkerFlag->empty())
    {
      vars.emplace_back("CMAKE_EXE_LINKER_FLAGS", *options.fastLinkerFlag);
      vars.emplace_back("CMAKE_SHARED_LINKER_FLAGS", *options.fastLinkerFlag);
      vars.emplace_back("CMAKE_MODULE_LINKER_FLAGS", *options.fastLinkerFlag);
    }

    if (options.withSqlite)
    {
      vars.emplace_back("VIX_ENABLE_DB", "ON");
      vars.emplace_back("VIX_DB_USE_SQLITE", "ON");
      vars.emplace_back("VIX_DB_REQUIRE_SQLITE", "ON");
    }

    if (options.withMySql)
    {
      vars.emplace_back("VIX_ENABLE_DB", "ON");
      vars.emplace_back("VIX_DB_USE_MYSQL", "ON");
      vars.emplace_back("VIX_DB_REQUIRE_MYSQL", "ON");
    }

    std::sort(
        vars.begin(),
        vars.end(),
        [](const auto &a, const auto &b)
        {
          return a.first < b.first;
        });

    return vars;
  }
} // namespace vix::engine
