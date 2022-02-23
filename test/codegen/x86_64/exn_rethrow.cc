// RUN: %clang -O2 -target llir_x86_64 -S -xc++ - -o - -O1

class Exception;

int f();

int test() {
  try {
    return f();
  } catch (...) {
    // CHECK: landing_pad.i64.i64
    // CHECK: __cxa_rethrow
    throw;
  }
}
