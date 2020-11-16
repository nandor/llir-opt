// RUN: %clang -target llir_powerpc64le-unknown-linux -xc - -S -O2 -o- | %opt - -triple powerpc64le-unknown-linux  -o=-

char return1()
{
  return 1;
}

long long return2()
{
  return 1;
}

float return3()
{
  return 1.0f;
}

double return4()
{
  return 3.0;
}
