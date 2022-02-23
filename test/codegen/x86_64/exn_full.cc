// RUN: %clang -O2 -target llir_x86_64 -S -xc++ - -o - -O1

#include <cassert>
#include <cstdio>

class A {};
class B {};
class C {};

bool destroyed = false;

class D { public: ~D() { destroyed = true; } };

void run(int x)
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
    puts("A");
  } catch (const B &a) {
    puts("B");
  } catch (const C &a) {
    puts("C");
  } catch (...) {
    puts("default");
  }
}

int main(int argc, char **argv)
{
  run(argc);
  assert(destroyed);
  return 0;
}
