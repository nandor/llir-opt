# GenM Optimiser

The GenM optimiser performs low-level cross-language optimisations on a
low-level IR which can be easily generated from various compilers.

## Setup

The opt, llvm, clang and ocaml projects should be checked out in the following folders:

```
<prefix>
├─ llvm               https://github.com/nandor/llvm-genm
│  └─ tools
│     └─ clang        https://github.com/nandor/clang-genm
├─ ocaml              https://github.com/nandor/ocaml-genm
├─ opt                https://github.com/nandor/opt-genm
├─ musl               git://git.musl-libc.org/musl (Linux only)
└─ dist               install prefix
```

### llvm and clang

To generate GenM using LLVM, the llvm-genm and clang-genm forks are required. To build:

```
mkdir <prefix>/llvm/MinSizeRel
cd <prefix>/llvm/MinSizeRel
cmake ..                               \
  -G Ninja                             \
  -DCMAKE_BUILD_TYPE=MinSizeRel        \
  -DCMAKE_INSTALL_PREFIX=<prefix>/dist \
  -DLLVM_TARGETS_TO_BUILD="X86;GenM"   \
  -DLLVM_ENABLE_DUMP=ON
ninja
ninja install
```

### opt

Debug builds are configured as follows:
```
mkdir <prefix>/opt/Debug
cd <prefix>/opt/Debug
cmake ..                               \
  -GNinja                              \
  -DCMAKE_BUILD_TYPE=Debug             \
  -DLLVM_DIR=<prefix>/dist             \
  -DCMAKE_INSTALL_PREFIX=<prefix>/dist
ninja
ninja install
```

### musl

The musl implementation of libc is required on Linux:

```
./configure --prefix=<prefix>/dist/musl
make
make install
sudo make install
```

When musl is used, ```<prefix>/dist/musl/bin``` must be added to $PATH.
The las command installs a link to the musl dynamic loader to `/lib/ld-musl-x86_64.so.1`.

### ocaml

To generate GenM from OCaml, the ocaml-genm fork is required and a symbolic link
to the llvm fork must be added to `$PATH` under the `genm-gcc` alias. The OCaml
compiler can be built using the following commands:

```
cd <prefix>/ocaml
export PATH=$PATH:<prefix>/dist/bin
./configure                     \
  --target genm                 \
  --target-bindir <prefix>/dist \
  --prefix <prefix>/dist        \
  -no-ocamldoc                  \
  -no-debugger                  \
  -no-instrumented-runtime      \
  -no-debug-runtime             \
  -no-pthread                   \
  -no-graph                     \
  -fPIC                         \
  -flambda                      \
  -no-cfi
make world.opt
make install
```

An installation of OCaml 4.07.1 is also required, obtained through opam 2.0:

```
opam switch 4.07.1
```

## Testing

To run the tests bundled with the project:

```
cd <prefix>/opt/Release
ninja
PATH=$PATH:<prefix>/dist/bin ../test.py
```
