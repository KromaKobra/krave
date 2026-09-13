# Guide · Build and run

**Sources:** `CMakePresets.json`, `flake.nix`, `nix/package.nix`, `scripts/build.sh`, `scripts/build.ps1`, `.vscode/tasks.json`

---

## Requirements

| | |
|---|---|
| Qt | 6.8+ with **Quick, QuickControls2, Multimedia, Concurrent, ShaderTools** and private Gui headers |
| Compiler | MSVC 2022+ (verified on Build Tools 2026, toolset 14.51) or GCC 13+ / Clang 17+ |
| CMake | 3.21+, Ninja |
| ffmpeg | on `PATH`, or set `MV_FFMPEG` to its full path |

**ffmpeg is required for import as well as export** — it is the decoder as well as the
encoder. Without it, both are disabled and the toolbar says so.

`GuiPrivate` is not optional: `QRhi` lives behind private Gui headers and the export path
needs it. `ShaderTools` provides `qt_add_shaders`.

---

## Linux / Nix

The flake pins the whole toolchain — Qt, cmake, ninja, ffmpeg — so nothing needs to be
installed globally.

```bash
nix build           # → ./result/bin/krave, ffmpeg baked onto its PATH
nix run             # build and launch
```

For day-to-day work use the build script. It **re-execs itself inside `nix develop`** when
cmake is not on `PATH`, so it works from a bare terminal:

```bash
./scripts/build.sh                  # debug build
./scripts/build.sh --config release
./scripts/build.sh --test           # build then run ctest
./scripts/build.sh --run            # build then launch
./scripts/build.sh --clean
```

Or drive the presets by hand from inside the shell:

```bash
nix develop
cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug
./build/linux-debug/bin/krave
```

> **An in-tree binary must be launched from inside the dev shell.** It is not wrapped, so
> it finds Qt's plugins and QML modules only through the `QT_PLUGIN_PATH` /
> `QML2_IMPORT_PATH` the shell exports. The `nix build` output **is** wrapped and runs
> anywhere.

The dev shell also carries `clangd` (it reads the `compile_commands.json` the preset
emits), `gdb`, `lldb`, `qmllint` and `ffmpeg`.

Building without Nix works too, given Qt 6.8+ and ffmpeg on `PATH`:

```bash
cmake --preset linux-debug && cmake --build --preset linux-debug
```

---

## Windows

`cmake` and `ninja` ship inside the VS Build Tools and are usually not on `PATH`, and Ninja
needs the MSVC environment. The script handles all of that:

```bash
pwsh ./scripts/build.ps1
pwsh ./scripts/build.ps1 -Config Release -Test
```

It writes a temporary batch file that calls `vcvars64`, configures, builds, and deletes
itself in a `finally`.

> The PowerShell script does **not** use the presets — it configures into `build/$Config`
> directly, whereas the presets use `build/$presetName`. Do not mix the two.

If you already have a Developer PowerShell with Qt on `CMAKE_PREFIX_PATH`, the presets work
directly:

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

---

## Presets

Four configure presets over two hidden bases, all Ninja, all emitting
`compile_commands.json`, all building into `build/<presetName>`:

| Preset | Build type | Host |
|---|---|---|
| `debug` | Debug | Windows |
| `release` | RelWithDebInfo | Windows |
| `linux-debug` | Debug | Linux |
| `linux-release` | RelWithDebInfo | Linux |

Build presets mirror all four. **Test presets exist only for `debug` and `linux-debug`.**

`windows-base` hard-codes `CMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64` and the ninja inside
VS Build Tools.

---

## Running it

```bash
./build/linux-debug/bin/krave        # build/Debug/bin/krave.exe on Windows
```

> **No command-line flags work.** `main.cpp` contains no `QCommandLineParser` and never
> inspects `argv`. The README's `krave --audio track.mp3` example and the headless
> `--render` mode both date from before `5fa7fa8` removed all CLI parsing. See
> [Known discrepancies](../README.md#known-discrepancies).

Import a track with `Ctrl+I` instead. The editor reopens the last project, the last track
and the volume automatically — that state lives in `session.json` under the platform
config dir, not in the project.
→ [ADR-0017](../decisions/0017-no-audio-path-in-project.md)

### Shortcuts

`Space` play/pause · `Ctrl+N/O/S/Shift+S` document · `Ctrl+I` import · `Ctrl+E` export ·
`Del` delete the selected node.

---

## VS Code / VSCodium

`.vscode/tasks.json` and `launch.json` carry both hosts; the Linux debug configs are
suffixed `(Linux)`. Five tasks, each delegating to the platform build script rather than to
cmake, because on neither host is the toolchain simply on `PATH`.

**Every build writes the dev shell's Qt environment to `build/<preset>/dev.env`**, which
the launch configs read as their `envFile` — that is what lets a debug session find the QML
modules without VS Code itself being started from the shell.

Debugging uses **CodeLLDB** (`vadimcn.vscode-lldb`), not `cppdbg`: that type comes from
`ms-vscode.cpptools`, which is proprietary, absent from Open VSX, and cannot be installed
on VSCodium at all.

On NixOS take CodeLLDB from nixpkgs rather than the extensions panel — the marketplace
build ships an unpatched prebuilt binary that will not run:

```nix
programs.vscode = {
  enable = true;
  package = pkgs.vscodium;
  mutableExtensionsDir = true;   # keeps hand-installed extensions working
  profiles.default.extensions = with pkgs.vscode-extensions; [ vadimcn.vscode-lldb ];
};
```

> Two launch configs (`launch.json:63` and `:137`) still pass `--render --audio … --out …`.
> They silently open the editor instead, because unknown arguments are ignored.

To install Krave system-wide, add the repo as a flake input in `/etc/nixos/flake.nix` and
put `inputs.krave.packages.${pkgs.system}.default` in `environment.systemPackages`.

---

## Targets

| Target | Kind | Links |
|---|---|---|
| `mvcore` | static lib | `Qt6::Core`, `Qt6::Concurrent` |
| `mvapp` | static lib | `mvcore`, `Qt6::Core`, `Qt6::Gui`, `Qt6::Qml` |
| `krave` | executable | `mvapp`, `mvcore`, + Quick, QuickControls2, Multimedia, GuiPrivate |
| `tst_fft` | test | `mvcore` |
| `tst_graph`, `tst_project`, `tst_session` | tests | `mvapp` |

`MV_BUILD_TESTS` (default ON) controls the test subdirectory, which is also skipped if
`Qt6::Test` is not found.

> **Adding a `.qml` file requires editing `app_qml_files` in `src/app/CMakeLists.txt`.**
> All 49 are listed explicitly; there is no glob.

---

## See also

- [testing-and-debugging](testing-and-debugging.md)
- [2 · Containers](../2-containers/README.md) — what links what and why
