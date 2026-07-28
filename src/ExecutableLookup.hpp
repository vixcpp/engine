/**
 *
 *  @file ExecutableLookup.cpp
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
#ifndef VIX_ENGINE_EXECUTABLE_LOOKUP_HPP
#define VIX_ENGINE_EXECUTABLE_LOOKUP_HPP

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace vix::engine::detail
{
  namespace fs = std::filesystem;

  inline bool is_executable_file(const fs::path &path)
  {
    std::error_code ec;
    if (!fs::exists(path, ec) || ec)
      return false;
    if (!fs::is_regular_file(path, ec) || ec)
      return false;

#ifdef _WIN32
    return true;
#else
    const fs::perms perms = fs::status(path, ec).permissions();
    if (ec)
      return false;

    constexpr fs::perms executable =
        fs::perms::owner_exec |
        fs::perms::group_exec |
        fs::perms::others_exec;

    return (perms & executable) != fs::perms::none;
#endif
  }

  inline std::vector<std::string> executable_names(std::string_view name)
  {
    std::vector<std::string> names;
    names.emplace_back(name);

#ifdef _WIN32
    const fs::path input(std::string(name));
    if (input.has_extension())
      return names;

    const char *pathext = std::getenv("PATHEXT");
    const std::string extensions =
        (pathext && *pathext) ? std::string(pathext) : ".COM;.EXE;.BAT;.CMD";

    std::size_t start = 0;
    while (start <= extensions.size())
    {
      std::size_t end = extensions.find(';', start);
      if (end == std::string::npos)
        end = extensions.size();

      std::string ext = extensions.substr(start, end - start);
      if (!ext.empty())
        names.emplace_back(std::string(name) + ext);

      start = end + 1;
    }
#endif

    return names;
  }

  inline bool executable_on_path(std::string_view name)
  {
    if (name.empty())
      return false;

    const char *pathEnv = std::getenv("PATH");
    if (!pathEnv || !*pathEnv)
      return false;

#ifdef _WIN32
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif

    const std::string_view pathValue(pathEnv);
    const std::vector<std::string> names = executable_names(name);

    std::size_t start = 0;
    while (start <= pathValue.size())
    {
      std::size_t end = pathValue.find(separator, start);
      if (end == std::string_view::npos)
        end = pathValue.size();

      const std::string_view dir = pathValue.substr(start, end - start);
      if (!dir.empty())
      {
        const fs::path directory{std::string(dir)};
        for (const std::string &candidateName : names)
        {
          if (is_executable_file(directory / candidateName))
            return true;
        }
      }

      start = end + 1;
    }

    return false;
  }
} // namespace vix::engine::detail

#endif
