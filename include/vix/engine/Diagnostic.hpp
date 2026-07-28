/**
 * @file Diagnostic.hpp
 *
 * Structured diagnostics produced by Vix Engine.
 */
#ifndef VIX_ENGINE_DIAGNOSTIC_HPP
#define VIX_ENGINE_DIAGNOSTIC_HPP

#include <filesystem>
#include <string>

namespace vix::engine
{
  namespace fs = std::filesystem;

  /**
   * @brief Severity of a structured engine diagnostic.
   */
  enum class DiagnosticSeverity
  {
    Info,
    Warning,
    Error
  };

  /**
   * @brief A structured message emitted while planning or executing work.
   *
   * Diagnostics are data only. Terminal formatting and color decisions remain
   * the responsibility of the CLI.
   */
  struct Diagnostic
  {
    DiagnosticSeverity severity{DiagnosticSeverity::Info};
    std::string code;
    std::string message;
    fs::path file;
    int line{0};
    int column{0};
  };

} // namespace vix::engine

#endif
