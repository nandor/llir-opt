// RUN: %clang -O2 -target llir_x86_64 -S -xc++ - -o - -O1

class A {};

int test() {
  throw A{};
}
