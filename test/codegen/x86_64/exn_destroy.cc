// RUN: %clang -O2 -target llir_x86_64 -S -xc++ - -o - -O1

class A { public: ~A(); };

int f();

int test() {
  A a{};
  return f();
  // CHECK: landing_pad.i64.i64
  // CHECK: _ZN1AD1Ev
}
