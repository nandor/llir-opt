// RUN: %clang -target llir_riscv64 -xc - -S -o- | %opt - -triple riscv64 -mcpu=sifive-u74 -o=-

double trunc(double);

double caml_trunc(double x)
{
  // CHECK: trunc@plt
  return trunc(x);
}
