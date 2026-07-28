/**
 * @file Options.hpp
 *
 * Stable options accepted by Vix Engine.
 */
#ifndef VIX_ENGINE_OPTIONS_HPP
#define VIX_ENGINE_OPTIONS_HPP

#include <filesystem>
#include <string>
#include <vector>

#include <vix/engine/BuildTools.hpp>

namespace vix::engine
{
  namespace fs = std::filesystem;

  /**
   * @brief Minimal execution options shared by build-like workflows.
   *
   * This structure intentionally excludes CLI-only concerns such as colors,
   * progress rendering, Cloud reporting, and command-line parsing.
   */
  struct Options
  {
    fs::path projectDir;
    fs::path buildDir;
    std::string preset{"dev-ninja"};
    std::string targetTriple;
    std::string sysroot;
    std::string buildTarget;
    std::vector<std::string> cmakeArgs;
    int jobs{0};
    bool clean{false};
    bool verbose{false};
    bool explain{false};
    bool useCache{true};
    bool linkStatic{false};
    LinkerMode linker{LinkerMode::Auto};
    LauncherMode launcher{LauncherMode::Auto};
  };

} // namespace vix::engine

#endif
