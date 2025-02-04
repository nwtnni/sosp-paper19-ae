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
              cp -r ../pkgconfig "$dev/lib/"
              cp -r ../include "$dev/"
            '';

            outputs = [
              "out"
              "dev"
            ];
          };
        };
      }
    );
}
