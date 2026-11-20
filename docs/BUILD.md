# Building MFG Fix NG LE

This document describes the **source archive supplied with Test Build 5**, not an independently verified GitHub CI build.

## Requirements

- Windows and Visual Studio **2022**, **MSVC v143**, **Win32/x86** development tools.
- Skyrim Legendary Edition **1.9.32.0.8**.
- **SKSE 1.7.3** SDK source, present at `Source/original-skse-sdk` in the supplied archive.
- For the Papyrus scripts, Skyrim LE Papyrus compiler and corresponding LE game/SKSE script sources.

## Native DLL

Extract the distribution archive and run a **Visual Studio 2022 Developer PowerShell**:

```powershell
cd Source/Port
./Build.ps1
```

The provided project is `MfgFixNGLE.vcxproj` (Release / Win32). It uses the port's own C++17 sources, LE SKSE SDK interfaces and a static CRT; it **does not** link CommonLibSSE-NG or Detours.

## Test harness

```powershell
cd Source/Port
./Build.ps1 -Harness
```

The supplied tests include `tests/harness.cpp`, `tests/legacy_tests.inc`, mutation scripts, fake-engine inputs in `Source/harness/core`, and engine evidence in `Source/Port/evidence`.

## Package layout

```text
SKSE/Plugins/MfgFixNGLE.dll
SKSE/Plugins/MfgFixNGLE.ini
Scripts/MfgConsoleFunc.pex
Scripts/MfgConsoleFuncExt.pex
Source/Port/
Source/harness/
Source/original-MfgFix/
Source/original-MfgFixNG/
Source/original-skse-sdk/
COPYING
EXCEPTIONS.md
README.txt
VALIDATION.txt
```

Do not mix the SSE project/toolchain with the LE build. The original upstream sources are historical and attribution references; **build the LE project under `Source/Port`**.

For installation instructions, see the [README](../README.md).