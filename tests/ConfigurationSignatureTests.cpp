#include <vix/engine/ConfigurationSignature.hpp>

#include <cassert>

namespace
{
  using namespace vix::engine;

  ExecutionPlan plan()
  {
    ExecutionPlan p;
    p.preset = Preset{"dev-ninja", "Ninja", "Debug", "build-ninja"};
    p.projectFingerprint = "fingerprint";
    return p;
  }
}

int main()
{
  {
    const std::string expected =
        "preset=dev-ninja\n"
        "generator=Ninja\n"
        "buildType=Debug\n"
        "static=0\n"
        "targetTriple=\n"
        "sysroot=\n"
        "useCache=1\n"
        "warningCheck=0\n"
        "linker=0\n"
        "launcher=0\n"
        "verbose=0\n"
        "cmakeVerbose=0\n"
        "projectFingerprint=fingerprint\n"
        "vars:\n"
        "rawCMakeArgs:\n";

    ConfigurationSignatureOptions options;
    assert(make_configuration_signature(plan(), options) == expected);
    assert(make_configuration_signature(plan(), options).back() == '\n');
    assert(make_configuration_signature(plan(), options) ==
           make_configuration_signature(plan(), options));
  }

  {
    ExecutionPlan p = plan();
    p.preset = Preset{"release", "Ninja", "Release", "build-release"};
    p.launcher = "ccache";
    p.fastLinkerFlag = "-fuse-ld=mold";
    p.cmakeVars = {
        {"Z_VAR", "last"},
        {"A_VAR", "first value"}};

    ConfigurationSignatureOptions options;
    options.linkStatic = true;
    options.targetTriple = "aarch64-linux-gnu";
    options.sysroot = "/opt/sys root";
    options.useCache = false;
    options.warningCheck = true;
    options.linker = LinkerMode::Mold;
    options.launcher = LauncherMode::Ccache;
    options.verbose = true;
    options.cmakeVerbose = true;
    options.rawCMakeArgs = {
        "-DFOO=bar baz",
        "-DQUOTE=\"yes\""};
    options.toolchainContent = "set(X 1)";

    const std::string expected =
        "preset=release\n"
        "generator=Ninja\n"
        "buildType=Release\n"
        "static=1\n"
        "targetTriple=aarch64-linux-gnu\n"
        "sysroot=/opt/sys root\n"
        "useCache=0\n"
        "warningCheck=1\n"
        "linker=2\n"
        "launcher=3\n"
        "verbose=1\n"
        "cmakeVerbose=1\n"
        "launcherTool=ccache\n"
        "linkerFlag=-fuse-ld=mold\n"
        "projectFingerprint=fingerprint\n"
        "vars:\n"
        "Z_VAR=last\n"
        "A_VAR=first value\n"
        "rawCMakeArgs:\n"
        "-DFOO=bar baz\n"
        "-DQUOTE=\"yes\"\n"
        "toolchain:\n"
        "set(X 1)\n";

    assert(make_configuration_signature(p, options) == expected);
  }

  {
    ConfigurationSignatureOptions options;
    options.linker = LinkerMode::Default;
    assert(make_configuration_signature(plan(), options).find("linker=1\n") != std::string::npos);
    options.linker = LinkerMode::Mold;
    assert(make_configuration_signature(plan(), options).find("linker=2\n") != std::string::npos);
    options.linker = LinkerMode::Lld;
    assert(make_configuration_signature(plan(), options).find("linker=3\n") != std::string::npos);

    options.launcher = LauncherMode::None;
    assert(make_configuration_signature(plan(), options).find("launcher=1\n") != std::string::npos);
    options.launcher = LauncherMode::Sccache;
    assert(make_configuration_signature(plan(), options).find("launcher=2\n") != std::string::npos);
    options.launcher = LauncherMode::Ccache;
    assert(make_configuration_signature(plan(), options).find("launcher=3\n") != std::string::npos);
  }

  {
    ConfigurationSignatureOptions options;
    options.toolchainContent = "ignored";
    assert(make_configuration_signature(plan(), options).find("toolchain:\n") == std::string::npos);

    options.targetTriple = "x86_64-linux-gnu";
    options.toolchainContent = "line\n";
    assert(make_configuration_signature(plan(), options).find("toolchain:\nline\n") != std::string::npos);
  }

  return 0;
}
