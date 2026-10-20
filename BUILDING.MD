# Building Menu Magician

Menu Magician is a Win32 DLL module for The Major BBS v10. It is built with
Microsoft Visual Studio against the MBBS v10 Module SDK.

## Prerequisites

- **Microsoft Visual Studio 2022** (or Build Tools 2022) with the C++
  desktop workload. Platform toolset **v143**.
- **Windows 10 SDK** (`WindowsTargetPlatformVersion` 10.0).
- **The Major BBS v10 Module SDK**
  (https://github.com/TheMajorBBS/MBBS-v10-module-SDK). It provides the
  headers (`inc/`) and import libraries (`lib/importlibs`, `lib/v10`).

> **Build for x86 (Win32) only. Never x64.** The Major BBS v10 is a 32-bit
> program and cannot load a 64-bit module.

## Setup

Copy `MBBSSDK.props.example` to `MBBSSDK.props` and set `MbbsSdkDir` to your
SDK folder (trailing backslash required). `MBBSSDK.props` is not committed.

## Message file

The module's text is in `SRC/GALMM/GALMM.MSG`, and `SRC/GALMM/GALMM.H`
holds the matching message numbers. `GALMM.MSG` contains raw ESC and 0x01
bytes (ANSI color and named text variables) and CP437 line-drawing
characters, and must keep CRLF line endings, so edit it with an editor that
preserves all of them. `GALMM.MST` is the same text with those bytes written
as readable `<ESC>`, `<TV:NAME>` and `<CHR:n>` tokens.

Message numbers are assigned in file order, starting from 0, and every
named entry counts, including the `LEVEL6` marker. If you add, remove or
reorder an entry, update `GALMM.H` to match and rebuild the DLL. Deploy the
DLL and `.MSG` together. The BBS compiles `GALMM.MCV` from the `.MSG`
itself.

## Build

In Visual Studio, open `GALMM.sln`, choose **Release** / **Win32**, and
build. Or from a command prompt:

```
MSBuild.exe GALMM.sln -p:Configuration=Release -p:Platform=Win32
```

The DLL is written to `BUILD/Release/GALMM.dll`.

## Tests

`TESTS\RUNTESTS.BAT` builds and runs the standalone test of the session
runner (`MMFIBER.C`). It compiles the real source against small stand-in
engine headers in `TESTS\FAKE` and needs only the Visual Studio C++ tools,
not a BBS. Everything else is tested on a running board.

## Packaging a release

1. Set the new version in `SRC/GALMM/GALMMDEF.H` (`GALMM_VER_*`,
   `GALMM_VERSION`) and `SRC/GALMM/GALMM.rc`, and update `CHANGELOG.MD` and
   the version history in `GALMM.RLN`.
2. Rebuild the Release configuration, and check that the DLL's version
   matches `GALMM_VERSION`.
3. Copy only the deployable files into `DIST/<version>/`: `GALMM.DLL`,
   `GALMM.MDF`, `GALMM.MSG` and `GALMM.RLN`. Zip them as
   `GALMM-<version>.zip`.

Never rebuild under a version number that has already been released.
Bump the version instead.

## Release checklist

- Project builds with no errors or warnings, and `TESTS\RUNTESTS.BAT` passes.
- Version numbers updated (`GALMMDEF.H`, `GALMM.rc`, `CHANGELOG.MD`,
  `GALMM.RLN`).
- `README.MD` updated.
- `DIST/<version>/` holds only the four deployable files, with CRLF line
  endings in the `.MSG`, `.MDF` and `.RLN`.
- DLL properties show the right product, version, company and copyright.
- Installation tested on a test board.
- No credentials, internal paths or internal notes in published files.
