#include <vix/engine/Process.hpp>

#include <sstream>

namespace vix::engine::process
{
  namespace
  {
    static std::string quote_display_arg(const std::string &arg)
    {
      if (arg.empty())
        return "\"\"";

      bool needsQuote = false;
      for (char c : arg)
      {
        if (c == ' ' || c == '\t' || c == '\n' || c == '"' || c == '\'')
        {
          needsQuote = true;
          break;
        }
      }

      if (!needsQuote)
        return arg;

      std::string out = "\"";
      for (char c : arg)
      {
        if (c == '"' || c == '\\')
          out.push_back('\\');
        out.push_back(c);
      }
      out.push_back('"');
      return out;
    }
  } // namespace

  bool Command::valid() const
  {
    return !argv.empty() && !argv.front().empty();
  }

  bool Result::success() const
  {
    return started && exitCode == 0;
  }

  std::string display_command(const Command &command)
  {
    std::ostringstream out;

    if (!command.workingDirectory.empty())
      out << "(cd " << quote_display_arg(command.workingDirectory.string()) << " && ";

    for (std::size_t i = 0; i < command.argv.size(); ++i)
    {
      if (i > 0)
        out << ' ';
      out << quote_display_arg(command.argv[i]);
    }

    if (!command.workingDirectory.empty())
      out << ")";

    return out.str();
  }
} // namespace vix::engine::process
