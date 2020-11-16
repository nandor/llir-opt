// RUN: %clang -target llir_x86_64-unknown-linux -xc - -S -O2 -o- | %opt - -triple x86_64-unknown-linux  -o=-

int test_arguments_int(
  int x0,
  int x1,
  int x2,
  int x3,
  int x4,
  int x5,
  int x6,
  int x7,
  int x8,
  int x9,
  int x10,
  int x11,
  int x12,
  int x13,
  int x14,
  int x15)
{
  return x5 + x15;
}

float test_arguments_float(
  float x0,
  float x1,
  float x2,
  float x3,
  float x4,
  float x5,
  float x6,
  float x7,
  float x8,
  float x9,
  float x10,
  float x11,
  float x12,
  float x13,
  float x14,
  float x15,
  int y)
{
  return x5 + x15;
}

float test_arguments_int_float(
  int i0, float f0,
  int i1, float f1,
  int i2, float f2,
  int i3, float f3,
  int i4, float f4,
  int i5, float f5,
  int i6, float f6,
  int i7, float f7,
  int i8, float f8,
  int i9, float f9,
  int i10, float f10,
  int i11, float f11,
  int i12, float f12,
  int i13, float f13,
  int i14, float f14,
  int i15, float f15)
{
  return i5 + f5 + i15 + f15;
}

float test_arguments_float_mix_int(
  int i0,
  int i1,
  int i2,
  int i3,
  float f1,
  int i4,
  float f2,
  float f3,
  float f4,
  float f5,
  float f6,
  float f7,
  float f8,
  float f9,
  float f10,
  float f11,
  float f12,
  float f13,
  int i5,
  float fstk0,
  float fstk1,
  float fstk2,
  int i6)
{
  return f1 + i4 + f13 + i5 + fstk0;
}
