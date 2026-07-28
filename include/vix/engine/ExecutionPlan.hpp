/**
 *
 *  @file ExecutionPlan.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Engine execution plan model
 *
 */
#ifndef VIX_ENGINE_EXECUTION_PLAN_HPP
#define VIX_ENGINE_EXECUTION_PLAN_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <vix/engine/Preset.hpp>

namespace vix::engine
{
  namespace fs = std::filesystem;

  /**
   * @brief Fully resolved execution plan for a build-like workflow.
   *
   * This is a neutral data model. It does not parse CLI arguments, inspect
   * the environment, run CMake, generate files, or print diagnostics.
   */
  struct ExecutionPlan
  {
    /**
     * @brief User project root directory.
     */
    fs::path userProjectDir;

    /**
     * @brief CMake source directory passed to `cmake -S`.
     */
    fs::path cmakeSourceDir;

    /**
     * @brief Transitional root project directory field.
     *
     * Existing CLI code still reads this field. New engine-facing code should
     * prefer userProjectDir or cmakeSourceDir depending on intent.
     */
    fs::path projectDir;

    /**
     * @brief Default target name used when no explicit build target is set.
     */
    std::string defaultTargetName;

    /**
     * @brief True when the active CMake project was generated from vix.app.
     */
    bool generatedFromVixApp{false};

    /**
     * @brief Resolved embedded preset.
     */
    Preset preset;

    /**
     * @brief Build directory used for configure/build artifacts.
     */
    fs::path buildDir;

    fs::path configureLog;  ///< Configure log path
    fs::path buildLog;      ///< Build log path
    fs::path sigFile;       ///< Configuration signature file path
    fs::path toolchainFile; ///< Generated toolchain file path

    fs::path sdkConfigDir;        ///< Resolved SDK CMake package directory
    std::string sdkResolutionError; ///< Fatal SDK resolution error

    /**
     * @brief Resolved CMake cache variables passed during configure.
     */
    std::vector<std::pair<std::string, std::string>> cmakeVars;

    std::string signature; ///< Configuration signature

    std::optional<std::string> launcher;       ///< Compiler launcher executable
    std::optional<std::string> fastLinkerFlag; ///< Fast linker compiler flag

    std::string projectFingerprint; ///< Fingerprint of important project files

    /**
     * @brief Check side-effect-free invariants required by a created plan.
     */
    bool valid() const;
  };

} // namespace vix::engine

#endif
