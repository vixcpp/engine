#include <vix/engine/Engine.hpp>

namespace vix::engine
{
  ExecutionResult Engine::execute(const Options &options) const
  {
    ExecutionResult result;

    if (options.projectDir.empty())
    {
      result.status = ExecutionStatus::Unsupported;
      result.exitCode = 2;
      result.diagnostics.push_back({
          DiagnosticSeverity::Error,
          "engine.project_dir_missing",
          "projectDir is required before engine execution can be planned",
          {},
          0,
          0});
      return result;
    }

    result.status = ExecutionStatus::Unsupported;
    result.exitCode = 2;
    result.events.push_back({
        EventKind::Message,
        "Vix Engine execution planning is not connected in this extraction phase",
        {}});
    return result;
  }

} // namespace vix::engine
