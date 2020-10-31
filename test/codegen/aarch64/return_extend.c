// RUN: %clang -target llir_aarch64 -xc - -S -o- | %opt - -triple aarch64 -o=-

char return_int8();
short return_int16();

unsigned char return_uint8();
unsigned short return_uint16();

int call_return_int8() {
  return return_int8() + 4;
}

int call_return_uint8() {
  return return_uint8() + 4;
}

int call_return_int16() {
  return return_int16() + 4;
}

int call_return_uint16() {
  return return_uint16() + 4;
}

extern void *ptr;

char return_int8() {
  return *((char *)ptr);
}

unsigned char return_uint8() {
  return *((unsigned char *)ptr);
}

short return_int16() {
  return *((short *)ptr);
}

unsigned short return_uint16() {
  return *((unsigned short *)ptr);
}
