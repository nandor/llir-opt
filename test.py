#!/usr/bin/env python3

import os
import subprocess
import tempfile
import sys
import shutil

PROJECT = os.path.dirname(os.path.abspath(__file__))
OPT_EXE = os.path.join(PROJECT, 'build', 'genm')
LNK_EXE = os.path.join(PROJECT, 'tools', 'genm-ld')
GCC_EXE = shutil.which('genm-gcc')
BIN_EXE = shutil.which('gcc')

def run_asm_test(path):
  """Runs an assembly test."""
  print(path)
  with tempfile.NamedTemporaryFile(suffix='.S') as output:
    subprocess.check_call([OPT_EXE, path, '-o', output.name])


def run_c_test(path):
  """Runs a C test."""

  def run_test(root):
    """Helper function to run a test."""
    genm_src = os.path.join(root, 'test.o')
    genm_lnk = os.path.join(root, 'test.genm')
    genm_obj = os.path.join(root, 'test.opt.o')
    genm_exe = os.path.join(root, 'test')

    # Build an executable, passing the source file through genm.
    subprocess.check_call([
        GCC_EXE,
        path,
        '-O2',
        '-fno-stack-protector',
        '-fomit-frame-pointer',
        '-o', genm_src
    ])
    subprocess.check_call([
        LNK_EXE,
        genm_src,
        '-o', genm_lnk
    ])
    subprocess.check_call([
        OPT_EXE,
        genm_lnk,
        '-o', genm_obj
    ])
    subprocess.check_call([
        BIN_EXE,
        genm_obj,
        '-o', genm_exe
    ])

    # Run the executable.
    subprocess.check_call([
      genm_exe
    ])

  print(path)
  with tempfile.TemporaryDirectory() as root:
    run_test(root)


def run_test(path):
  """Runs a test, detecting type from the extension."""
  if path.endswith('.S'):
    return run_asm_test(path)
  if path.endswith('.c'):
    return run_c_test(path)
  raise Exception('Invalid path type')


def run_tests(tests):
  """Runs a test suite."""
  for test in tests:
    run_test(os.path.abspath(test))


if __name__ == '__main__':
  if sys.argv[1:]:
    # Run all tests specified in command line arguments.
    run_tests(sys.argv[1:])
  else:
    # Run all tests in the test directory.
    def find_tests():
      for directory, _, files in os.walk(os.path.join(PROJECT, 'test')):
        for file in files:
          yield os.path.join(directory, file)

    run_tests(find_tests())
