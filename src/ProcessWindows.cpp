#include <vix/engine/Process.hpp>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace vix::engine::process
{
  namespace
  {
    static std::wstring widen(const std::string &value)
    {
      if (value.empty())
        return {};

      const int size = MultiByteToWideChar(
          CP_UTF8,
          0,
          value.c_str(),
          static_cast<int>(value.size()),
          nullptr,
          0);

      if (size <= 0)
        return std::wstring(value.begin(), value.end());

      std::wstring out(static_cast<std::size_t>(size), L'\0');
      MultiByteToWideChar(
          CP_UTF8,
          0,
          value.c_str(),
          static_cast<int>(value.size()),
          out.data(),
          size);
      return out;
    }

    static std::string narrow(const std::wstring &value)
    {
      if (value.empty())
        return {};

      const int size = WideCharToMultiByte(
          CP_UTF8,
          0,
          value.c_str(),
          static_cast<int>(value.size()),
          nullptr,
          0,
          nullptr,
          nullptr);

      if (size <= 0)
        return std::string(value.begin(), value.end());

      std::string out(static_cast<std::size_t>(size), '\0');
      WideCharToMultiByte(
          CP_UTF8,
          0,
          value.c_str(),
          static_cast<int>(value.size()),
          out.data(),
          size,
          nullptr,
          nullptr);
      return out;
    }

    static std::wstring quote_windows_arg(const std::string &arg)
    {
      if (arg.empty())
        return L"\"\"";

      bool needsQuotes = false;
      for (char c : arg)
      {
        if (c == ' ' || c == '\t' || c == '"' || c == '\n')
        {
          needsQuotes = true;
          break;
        }
      }

      const std::wstring wide = widen(arg);
      if (!needsQuotes)
        return wide;

      std::wstring out = L"\"";
      std::size_t backslashes = 0;

      for (wchar_t c : wide)
      {
        if (c == L'\\')
        {
          ++backslashes;
          continue;
        }

        if (c == L'"')
        {
          out.append(backslashes * 2 + 1, L'\\');
          out.push_back(c);
          backslashes = 0;
          continue;
        }

        out.append(backslashes, L'\\');
        backslashes = 0;
        out.push_back(c);
      }

      out.append(backslashes * 2, L'\\');
      out.push_back(L'"');
      return out;
    }

    static std::wstring command_line(const std::vector<std::string> &argv)
    {
      std::wstring out;

      for (std::size_t i = 0; i < argv.size(); ++i)
      {
        if (i > 0)
          out.push_back(L' ');
        out += quote_windows_arg(argv[i]);
      }

      return out;
    }

    static std::string last_error_message(const char *prefix)
    {
      const DWORD code = GetLastError();
      LPWSTR buffer = nullptr;

      FormatMessageW(
          FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
          nullptr,
          code,
          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
          reinterpret_cast<LPWSTR>(&buffer),
          0,
          nullptr);

      std::ostringstream out;
      out << prefix << ": ";
      if (buffer)
      {
        out << narrow(buffer);
        LocalFree(buffer);
      }
      else
      {
        out << "Windows error " << code;
      }

      return out.str();
    }

    static std::wstring build_environment_block(const Command &command)
    {
      if (command.environment.empty())
        return {};

      std::vector<std::wstring> entries;

      if (command.inheritEnvironment)
      {
        LPWCH env = GetEnvironmentStringsW();
        if (env)
        {
          for (LPWCH it = env; *it; it += wcslen(it) + 1)
            entries.emplace_back(it);
          FreeEnvironmentStringsW(env);
        }
      }

      for (const auto &kv : command.environment)
      {
        const std::wstring key = widen(kv.first);
        const std::wstring prefix = key + L"=";

        entries.erase(
            std::remove_if(
                entries.begin(),
                entries.end(),
                [&](const std::wstring &entry)
                {
                  return _wcsnicmp(entry.c_str(), prefix.c_str(), prefix.size()) == 0;
                }),
            entries.end());

        entries.push_back(prefix + widen(kv.second));
      }

      std::sort(entries.begin(), entries.end());

      std::wstring block;
      for (const auto &entry : entries)
      {
        block += entry;
        block.push_back(L'\0');
      }
      block.push_back(L'\0');
      return block;
    }
  } // namespace

  int normalize_exit_code(int raw) noexcept
  {
    return raw;
  }

  Result execute(const Command &command)
  {
    Result result;
    result.displayCommand = display_command(command);

    if (!command.valid())
    {
      result.exitCode = 127;
      result.errorMessage = "Empty process command.";
      return result;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;

    if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
    {
      result.exitCode = 127;
      result.errorMessage = last_error_message("CreatePipe failed");
      return result;
    }

    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION info{};
    std::wstring cmd = command_line(command.argv);
    std::wstring cwd = command.workingDirectory.empty()
                           ? std::wstring()
                           : command.workingDirectory.wstring();
    std::wstring envBlock = build_environment_block(command);

    const BOOL ok = CreateProcessW(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        TRUE,
        envBlock.empty() ? 0 : CREATE_UNICODE_ENVIRONMENT,
        envBlock.empty() ? nullptr : envBlock.data(),
        cwd.empty() ? nullptr : cwd.c_str(),
        &startup,
        &info);

    CloseHandle(writePipe);

    if (!ok)
    {
      result.exitCode = 127;
      result.errorMessage = last_error_message("CreateProcessW failed");
      CloseHandle(readPipe);
      return result;
    }

    result.started = true;

    char buffer[8192];
    DWORD read = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0)
      result.output.append(buffer, buffer + read);

    CloseHandle(readPipe);

    WaitForSingleObject(info.hProcess, INFINITE);

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(info.hProcess, &exitCode))
      exitCode = 127;

    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);

    result.exitCode = normalize_exit_code(static_cast<int>(exitCode));
    result.producedOutput = !result.output.empty();
    return result;
  }
} // namespace vix::engine::process

#endif
