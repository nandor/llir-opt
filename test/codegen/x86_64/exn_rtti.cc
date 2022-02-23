// RUN: %clang -O2 -target llir_x86_64 -S -xc++ - -o - -O1

class A {};
class B {};
class C {};

int f();

int test() {
  try {
    return f();
  } catch (const A &x) {
    return 1;
  } catch (const B &y) {
    return 2;
  } catch (const C &z) {
    return 3;
  } catch (...) {
    return 4;
  }
}
