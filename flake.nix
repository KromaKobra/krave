{
  description = "krave - layered, frequency-reactive music visualizer";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems
        (system: f system nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (system: pkgs: {
        default = self.packages.${system}.krave;
        krave = pkgs.callPackage ./nix/package.nix { };
      });

      apps = forAllSystems (system: pkgs: {
        default = {
          type = "app";
          program = "${self.packages.${system}.krave}/bin/krave";
          meta.description = "Layered, frequency-reactive music visualizer";
        };
      });

      devShells = forAllSystems (system: pkgs: {
        default = pkgs.mkShell {
          # Pulls in the Qt modules, cmake/ninja and wrapQtAppsHook from the
          # package, so the shell sees the same toolchain the derivation builds
          # with. wrapQtAppsHook also exports QT_PLUGIN_PATH / QML2_IMPORT_PATH
          # here, which is what makes a locally-built ./build/bin/krave run.
          inputsFrom = [ self.packages.${system}.krave ];

          packages = with pkgs; [
            ffmpeg
            clang-tools # clangd, reads compile_commands.json
            gdb
            lldb # what the VS Code launch configs drive, via CodeLLDB
            qt6.qtdeclarative # qmllint / qmlformat
          ];

          # An in-tree build is not wrapped, so it finds Qt's plugins and QML
          # modules only through these. They must point at the Qt this shell
          # builds against -- inheriting them from the ambient user profile
          # happens to work while both resolve to the same nixpkgs, and breaks
          # silently the moment they drift apart.
          shellHook =
            let
              qtPluginPath = pkgs.lib.makeSearchPathOutput "out" "lib/qt-6/plugins" [
                pkgs.qt6.qtbase
                pkgs.qt6.qtmultimedia
                pkgs.qt6.qtwayland
                pkgs.qt6.qtdeclarative
              ];
              qmlImportPath = pkgs.lib.makeSearchPathOutput "out" "lib/qt-6/qml" [
                pkgs.qt6.qtdeclarative
                pkgs.qt6.qtmultimedia
              ];
            in
            ''
              export QT_PLUGIN_PATH="${qtPluginPath}"
              export QML2_IMPORT_PATH="${qmlImportPath}"
              export QML_IMPORT_PATH="$QML2_IMPORT_PATH"
              export CMAKE_EXPORT_COMPILE_COMMANDS=ON

              echo "krave dev shell - Qt ${pkgs.qt6.qtbase.version}, $(cmake --version | head -1)"
              echo "  ./scripts/build.sh --test    (or: cmake --build --preset linux-debug)"
            '';
        };
      });

      formatter = forAllSystems (system: pkgs: pkgs.nixpkgs-fmt);
    };
}
