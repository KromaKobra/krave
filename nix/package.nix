{ lib
, stdenv
, cmake
, ninja
, qt6
, ffmpeg
, nix-gitignore
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "krave";
  version = "0.0.0";

  src = nix-gitignore.gitignoreSource [ "nix/" "flake.nix" "flake.lock" ] ./..;

  nativeBuildInputs = [
    cmake
    ninja
    qt6.wrapQtAppsHook
    # qsb, invoked by qt_add_shaders() for the blend-mode fragment shaders.
    qt6.qtshadertools
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtdeclarative # Quick, QuickControls2, Particles, Shapes
    qt6.qtmultimedia
    qt6.qtshadertools
    # Not referenced by the build, but it carries the Wayland platform plugin;
    # without it in the wrap the app can only start under XWayland.
    qt6.qtwayland
  ];

  cmakeFlags = [
    (lib.cmakeBool "MV_BUILD_TESTS" finalAttrs.finalPackage.doCheck)
  ];

  doCheck = true;

  # tests/CMakeLists.txt pins QT_QPA_PLATFORM=offscreen on tst_project, which is
  # what makes this work with no display in the sandbox.
  checkPhase = ''
    runHook preCheck
    ctest --output-on-failure
    runHook postCheck
  '';

  # ffmpeg is the decoder as well as the encoder, and is looked up via
  # QStandardPaths::findExecutable. Put it on the wrapped app's PATH so the
  # binary works without ffmpeg installed system-wide; MV_FFMPEG still
  # overrides it at runtime.
  qtWrapperArgs = [
    "--suffix PATH : ${lib.makeBinPath [ ffmpeg ]}"
  ];

  meta = {
    description = "Layered, frequency-reactive music visualizer with ffmpeg video export";
    homepage = "https://github.com/KromaKobra/eq_visualizer";
    # No LICENSE file in the repo yet - add one and set this when you pick one.
    mainProgram = "krave";
    platforms = lib.platforms.linux;
  };
})
