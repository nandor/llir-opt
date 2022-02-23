// RUN: %clang -O2 -target llir_x86_64 -S -xc++ - -o - -O1

#include <assert.h>

class A {};
class B {};
class C {};

bool destroyed = false;

class D { public: ~D() { destroyed = true; } };

int run(int x)
{
  D d;

  try {
    switch (x) {
      case 2: throw A{};
      case 3: throw B{};
      case 4: throw C{};
      default: throw 5;
    }
  } catch (const A &a) {
    return 1;
  } catch (const B &a) {
    return 2;
  } catch (const C &a) {
    return 3;
  } catch (...) {
    return 4;
  }
}

int main(int argc, char **argv)
{
  int ret = run(argc);
  assert(destroyed);
  return ret;
}
