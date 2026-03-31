{ pkgs ? import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/refs/tags/24.05.tar.gz") {} }:

let
  rust_overlay = import (builtins.fetchTarball "https://github.com/oxalica/rust-overlay/archive/master.tar.gz");
  pkgs = import <nixpkgs> { overlays = [ rust_overlay ]; };
  rustVersion = "latest";
  #rustVersion = "1.62.0";
  rust = pkgs.rust-bin.stable.${rustVersion}.default.override {
    extensions = [
      "rust-src" # for rust-analyzer
      "rust-analyzer"
    ];
  };
in

pkgs.mkShell {
  name = "spdm-playground";
  
  buildInputs = with pkgs; [
    python3
    libudev-zero
    boost.dev
    boost
    rdma-core
  ];

  nativeBuildInputs = with pkgs; [
    # kernel build
    getopt
    flex
    bison
    gcc
    gnumake
    bc
    pkg-config
    binutils
    elfutils
    openssl

    # bootable debian image
    debootstrap

    # SLIRP networking QEMU
    libslirp

    # QEMU build stuff
    python3.pkgs.sphinx
    python3.pkgs.sphinx-rtd-theme
    python313Packages.distlib
    meson
    ninja
    glib
    pixman
    json_c
    cmocka

    # SPDM utils build stuff
    cmake
    pciutils
    libudev-zero
    # cargo
    # rustc
    # rustfmt
    # rust
    # rustPlatform.bindgenHook
    # cbor-diag
    # clang
    # libclang

    rdma-core

    python3.pkgs.numpy
    python3.pkgs.pandas
    python3.pkgs.seaborn
    python3.pkgs.matplotlib

    numactl
  ];

  shellHook = ''
    export Boost_INCLUDE_DIR="${pkgs.boost.dev}/include"
    export BOOST_ROOT="${pkgs.boost}"
  '';


  hardeningDisable = [ "all" ];
}

