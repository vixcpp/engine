#include <vix/engine/Process.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#include <csignal>
#endif

using namespace vix::engine::process;

namespace
{
  namespace fs = std::filesystem;

  static fs::path selfPath;

  struct TempDir
  {
    fs::path path;

    TempDir()
    {
      path = fs::temp_directory_path() /
             ("vix-engine-process-test-" + std::to_string(std::rand()));
      fs::remove_all(path);
      fs::create_directories(path);
    }

    ~TempDir()
    {
      std::error_code ec;
      fs::remove_all(path, ec);
    }
  };

  static void require(bool condition, const std::string &message)
  {
    if (!condition)
      throw std::runtime_error(message);
  }

  static Command self_command(std::vector<std::string> args)
  {
    Command command;
    command.argv.push_back(selfPath.string());
    command.argv.insert(command.argv.end(), args.begin(), args.end());
    return command;
  }

  static void test_empty_command()
  {
    const Result result = execute(Command{});
    require(!result.success(), "empty command fails");
    require(!result.started, "empty command not started");
    require(result.exitCode == 127, "empty command exit");
    require(!result.errorMessage.empty(), "empty command error");
  }

  static void test_success_and_nonzero()
  {
    Result ok = execute(self_command({"--child-exit", "0"}));
    require(ok.started, "successful command started");
    require(ok.success(), "successful command success");

    Result failed = execute(self_command({"--child-exit", "7"}));
    require(failed.started, "failed command started");
    require(!failed.success(), "failed command not success");
    require(failed.exitCode == 7, "failed command exit code");
  }

  static void test_missing_executable()
  {
    Command command;
    command.argv = {"definitely-missing-vix-engine-process-test-executable"};

    const Result result = execute(command);
    require(!result.success(), "missing executable fails");
    require(result.exitCode == 127, "missing executable exit");
    require(!result.errorMessage.empty(), "missing executable error");
  }

  static void test_stdout_stderr_and_output_flag()
  {
    Result stdoutResult = execute(self_command({"--child-stdout", "hello"}));
    require(stdoutResult.success(), "stdout command success");
    require(stdoutResult.output == "hello", "stdout captured");
    require(stdoutResult.producedOutput, "stdout produced output");

    Result stderrResult = execute(self_command({"--child-stderr", "err"}));
    require(stderrResult.success(), "stderr command success");
    require(stderrResult.output == "err", "stderr captured");
    require(stderrResult.producedOutput, "stderr produced output");

    Result noOutput = execute(self_command({"--child-exit", "0"}));
    require(noOutput.success(), "no output success");
    require(!noOutput.producedOutput, "no output flag");
  }

  static void test_merged_output_order()
  {
    const Result result = execute(self_command({"--child-mixed-output"}));
    require(result.success(), "mixed output success");
    require(result.output == "out1\nerr1\nout2\nerr2\n", "merged output order");
  }

  static void test_working_directory()
  {
    TempDir temp;
    Command command = self_command({"--child-pwd"});
    command.workingDirectory = temp.path;

    const Result result = execute(command);
    require(result.success(), "working directory success");
    require(result.output.find(temp.path.string()) != std::string::npos, "working directory propagated");
  }

  static void test_arguments_with_spaces_and_quotes()
  {
    const Result result =
        execute(self_command({"--child-print-args", "hello world", "quote\"value"}));

    require(result.success(), "argument command success");
    require(result.output.find("[hello world]") != std::string::npos, "space argument preserved");
    require(result.output.find("[quote\"value]") != std::string::npos, "quote argument preserved");
  }

  static void test_environment()
  {
    Command inherited = self_command({"--child-print-env", "PATH"});
    const Result inheritedResult = execute(inherited);
    require(inheritedResult.success(), "inherited env success");
    require(!inheritedResult.output.empty(), "inherited PATH visible");

    Command override = self_command({"--child-print-env", "VIX_ENGINE_PROCESS_TEST"});
    override.environment.emplace_back("VIX_ENGINE_PROCESS_TEST", "override value");
    const Result overrideResult = execute(override);
    require(overrideResult.success(), "env override success");
    require(overrideResult.output == "override value", "env override visible");

    Command disabled = self_command({"--child-print-env", "PATH"});
    disabled.inheritEnvironment = false;
    const Result disabledResult = execute(disabled);
    require(disabledResult.success(), "disabled env success");
    require(disabledResult.output.empty(), "disabled env omits parent PATH");
  }

  static void test_large_output()
  {
    const Result result = execute(self_command({"--child-large-output"}));
    require(result.success(), "large output success");
    require(result.output.size() >= 1024 * 1024, "large output captured");
  }

  static void test_display_command()
  {
    Command command = self_command({"--child-print-args", "hello world"});
    command.workingDirectory = "/tmp/some path";
    const std::string display = display_command(command);

    require(display.find("hello world") != std::string::npos, "display includes spaced arg");
    require(display.find("cd") != std::string::npos, "display includes cwd");
  }

  static void test_normalize_exit_code()
  {
#ifdef _WIN32
    require(normalize_exit_code(12) == 12, "windows normalize");
#else
    int status = 0;
    require(normalize_exit_code(-1) == 127, "normalize wait failure");
    require(normalize_exit_code(status) == 0, "normalize zero status");
#endif
  }

  static void test_signal_termination()
  {
#ifndef _WIN32
    const Result result = execute(self_command({"--child-signal"}));
    require(!result.success(), "signal command fails");
    require(result.exitCode == 128 + SIGTERM, "signal normalized");
#endif
  }

  static void test_repeated_and_concurrent()
  {
    for (int i = 0; i < 3; ++i)
    {
      const Result result = execute(self_command({"--child-stdout", "repeat"}));
      require(result.success(), "repeated success");
      require(result.output == "repeat", "repeated output");
    }

    auto first = std::async(std::launch::async, []()
                            { return execute(self_command({"--child-stdout", "a"})); });
    auto second = std::async(std::launch::async, []()
                             { return execute(self_command({"--child-stdout", "b"})); });

    require(first.get().success(), "concurrent first");
    require(second.get().success(), "concurrent second");
  }

  static int child_main(int argc, char **argv)
  {
    const std::string mode = argc > 1 ? argv[1] : "";

    if (mode == "--child-exit")
      return argc > 2 ? std::atoi(argv[2]) : 0;

    if (mode == "--child-stdout")
    {
      if (argc > 2)
        std::cout << argv[2];
      return 0;
    }

    if (mode == "--child-stderr")
    {
      if (argc > 2)
        std::cerr << argv[2];
      return 0;
    }

    if (mode == "--child-mixed-output")
    {
      std::cout << "out1\n" << std::flush;
      std::cerr << "err1\n" << std::flush;
      std::cout << "out2\n" << std::flush;
      std::cerr << "err2\n" << std::flush;
      return 0;
    }

    if (mode == "--child-pwd")
    {
      std::cout << fs::current_path().string();
      return 0;
    }

    if (mode == "--child-print-args")
    {
      for (int i = 2; i < argc; ++i)
        std::cout << "[" << argv[i] << "]";
      return 0;
    }

    if (mode == "--child-print-env")
    {
      if (argc > 2)
      {
        const char *value = std::getenv(argv[2]);
        if (value)
          std::cout << value;
      }
      return 0;
    }

    if (mode == "--child-large-output")
    {
      std::string chunk(4096, 'x');
      for (int i = 0; i < 256; ++i)
        std::cout << chunk;
      return 0;
    }

    if (mode == "--child-signal")
    {
#ifndef _WIN32
      std::raise(SIGTERM);
#endif
      return 1;
    }

    return 2;
  }
} // namespace

int main(int argc, char **argv)
{
  if (argc > 1 && std::string(argv[1]).rfind("--child-", 0) == 0)
    return child_main(argc, argv);

  try
  {
    selfPath = fs::absolute(argv[0]).lexically_normal();

    test_empty_command();
    test_success_and_nonzero();
    test_missing_executable();
    test_stdout_stderr_and_output_flag();
    test_merged_output_order();
    test_working_directory();
    test_arguments_with_spaces_and_quotes();
    test_environment();
    test_large_output();
    test_display_command();
    test_normalize_exit_code();
    test_signal_termination();
    test_repeated_and_concurrent();
  }
  catch (const std::exception &ex)
  {
    std::cerr << "ProcessTests failed: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}
