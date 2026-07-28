/**
 * @file Process.hpp
 *
 * Structured process execution for Vix Engine.
 */
#ifndef VIX_ENGINE_PROCESS_HPP
#define VIX_ENGINE_PROCESS_HPP

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace vix::engine::process
{
  namespace fs = std::filesystem;

  /**
   * @brief Structured command to execute.
   *
   * argv[0] is the executable. Each argv entry is passed as one process
   * argument and is never shell-split by the engine.
   */
  struct Command
  {
    std::vector<std::string> argv; ///< Executable and arguments
    fs::path workingDirectory;     ///< Optional child working directory

    /**
     * @brief Environment overrides applied to the child process.
     *
     * Values replace existing variables with the same name. They do not mutate
     * the parent process environment.
     */
    std::vector<std::pair<std::string, std::string>> environment;

    bool inheritEnvironment{true}; ///< Include the parent environment
    bool mergeStdErr{true};        ///< Capture stderr in stdout order when true

    /**
     * @brief Check whether the command has an executable.
     */
    bool valid() const;
  };

  /**
   * @brief Structured process result.
   */
  struct Result
  {
    bool started{false};        ///< true after the child process starts
    int exitCode{0};            ///< Normalized process exit code
    std::string displayCommand; ///< Human-readable command representation
    std::string output;         ///< Captured stdout and optionally stderr
    std::string errorMessage;   ///< Spawn or setup error, when available
    bool producedOutput{false}; ///< true if output was captured

    /**
     * @brief Check whether the process started and exited successfully.
     */
    bool success() const;
  };

  /**
   * @brief Execute a structured command and capture output.
   */
  Result execute(const Command &command);

  /**
   * @brief Normalize a platform raw exit status into a process exit code.
   */
  [[nodiscard]] int normalize_exit_code(int raw) noexcept;

  /**
   * @brief Build a diagnostic-only display command.
   */
  std::string display_command(const Command &command);

} // namespace vix::engine::process

#endif
