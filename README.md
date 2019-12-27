# GenM Optimiser

The GenM optimiser performs low-level cross-language optimisations on a
low-level IR which can be easily generated from various compilers.

## Setup

The opt, llvm, clang and ocaml projects should be checked out in the following folders:

```
$PREFIX
├─ llvm               https://github.com/nandor/llvm-genm
│  └─ tools
│     └─ clang        https://github.com/nandor/clang-genm
├─ ocaml              https://github.com/nandor/ocaml-genm
├─ opt                https://github.com/nandor/opt-genm
├─ musl               git://git.musl-libc.org/musl (Linux only)
└─ dist               install prefix
```
`PREFIX` and `PATH` should be set in the shell's init script.

### llvm and clang

To generate GenM using LLVM, the llvm-genm and clang-genm forks are required. To build:

```
mkdir $PREFIX/llvm/MinSizeRel
cd $PREFIX/llvm/MinSizeRel
cmake ..                                  \
  -G Ninja                                \
  -DCMAKE_BUILD_TYPE=MinSizeRel           \
  -DCMAKE_INSTALL_PREFIX=$PREFIX/dist     \
  -DLLVM_TARGETS_TO_BUILD="X86;GenM"      \
  -DLLVM_ENABLE_DUMP=ON                   \
  -DLLVM_ENABLE_BINDINGS=OFF              \
  -DLLVM_ENABLE_OCAMLDOC=OFF              \
  -DCLANG_RESOURCE_DIR=../lib/clang/8.0.0
ninja
ninja install
```

On macOS, the following flag is also required:

```
  -DDEFAULT_SYSROOT=<path>
```

They must point to the same directories as the system compiler (`clang -###`).

### opt

Debug builds are configured as follows:
```
mkdir $PREFIX/opt/Debug
cd $PREFIX/opt/Debug
cmake ..                                  \
  -GNinja                                 \
  -DCMAKE_BUILD_TYPE=Debug                \
  -DLLVM_DIR=$PREFIX/dist/lib/cmake/llvm \
  -DCMAKE_INSTALL_PREFIX=$PREFIX/dist
ninja
ninja install
```

### musl

The musl implementation of libc is required on Linux:

```
./configure --prefix=$PREFIX/dist/musl
make
make install
sudo make install
```

When musl is used, ```$PREFIX/dist/musl/bin``` must be added to $PATH.
The las command installs a link to the musl dynamic loader to `/lib/ld-musl-x86_64.so.1`.

### ocaml

To generate GenM from OCaml, the ocaml-genm fork is required and a symbolic link
to the llvm fork must be added to `$PATH` under the `genm-gcc` alias. The OCaml
compiler can be built using the following commands:

```
cd $PREFIX/ocaml
export PATH=$PATH:$PREFIX/dist/bin
./configure                        \
  --target genm                    \
  --prefix $PREFIX/dist            \
  -no-debugger                     \
  -no-instrumented-runtime         \
  -no-debug-runtime                \
  -no-graph                        \
  -no-shared-libs                  \
  -fPIC                            \
  -flambda                         \
  -no-cfi
make world.opt
make install
```

### opam

To compile more OCaml packages with the new compiler, the compiler can be installed with dune:

```
cd $PREFIX/ocaml
opam switch create 4.07.1+genm --empty
opam pin add ocaml-variants.4.07.1+genm .
opam install ocaml-variants.4.07.1+genm
```

Use the following opam configuration for the custom compiler, substituting prefix:

```
opam-version: "2.0"
version: "4.07.1+genm"
synopsis: "4.07.01 with the genm backend"
maintainer: "n@ndor.email"
authors: "n@ndor.email"
homepage: "https://github.com/nandor/ocaml-genm"
bug-reports: "https://github.com/nandor/ocaml-genm/issues"
dev-repo: "git+file://$PREFIX/ocaml#master"
depends: [
  "ocaml" { = "4.07.1" & post }
  "base-unix" {post}
  "base-bigarray" {post}
  "base-threads" {post}
]
conflict-class: "ocaml-core-compiler"
flags: compiler
build: [
  [
    "./configure"
      "--target" "genm"
      "--target-bindir" bin
      "--prefix" prefix
      "-O1"
      "-no-debugger" "-no-instrumented-runtime" "-no-cfi"
      "-no-debug-runtime" "-no-graph" "-fPIC" "-flambda"
      "-no-shared-libs"
  ]
  [ make "world" "-j%{jobs}%"]
  [ make "world.opt" "-j%{jobs}%"]
]
install: [make "install"]
url {
  src: "git+file://$PREFIX/ocaml#master"
}
```

With `opam`, other packages, such as `dune`, can be installed:

```
opam install dune
```

If `$PREFIX` is not in the user's home folder, initialise `opam` without sandboxing:

```
opam init --disable-sandboxing
```

## Testing

To run the tests bundled with the project:

```
cd $PREFIX/opt/Release
ninja
PATH=$PREFIX/dist/bin:$PATH ../test.py
```
