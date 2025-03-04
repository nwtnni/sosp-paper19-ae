# https://fasterthanli.me/series/building-a-rust-service-with-nix/part-10
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
      with pkgs; {
        packages = rec {
          default = cxlmalloc;

          cxlmalloc = stdenv.mkDerivation {
            name = "cxlmalloc";
            src = ./.;
            nativeBuildInputs = [
              clang-tools
              cmake
              (mimalloc.overrideAttrs (prev: {
                cmakeFlags = prev.cmakeFlags ++ [ "-DMI_OVERRIDE=OFF" ];
              }))
              numactl
              pkg-config
              tbb
            ];

            installPhase = /* bash */ ''
              mkdir -p "$out/lib"
              local prefix="libcxlmalloc"

              for name in "''${prefix}.a" "''${prefix}_static.a" "''${prefix}_dynamic.so"; do
                cp "$name" "$out/lib/$name"
              done

              mkdir -p "$dev/lib"

              sed -i -e "s|@out@|''${out}|g" ../pkgconfig/*.pc
              sed -i -e "s|@dev@|''${dev}|g" ../pkgconfig/*.pc

              cp -r ../pkgconfig "$dev/lib/"
              cp -r ../include "$dev/"

              cp -r ../include "$source/"
              cp -r ../src "$source/"
            '';

            outputs = [
              "out"
              "dev"
              "source"
            ];
          };
        };
      }
    );
}
