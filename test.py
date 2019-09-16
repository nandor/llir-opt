#!/usr/bin/env python3

import argparse
import os
import platform
import shutil
import subprocess
import sys
import tempfile

IS_DEBUG = 'Debug' in os.getcwd()
PROJECT = os.path.dirname(os.path.abspath(__file__))
OPT_EXE = os.path.join(PROJECT, 'Debug' if IS_DEBUG else 'Release', 'genm')
LINK_EXE = os.path.join(PROJECT, 'tools', 'genm-ld')
GENM_GCC_EXE = shutil.which('genm-gcc')
MUSL_GCC_EXE = shutil.which('musl-gcc')
HOST_GCC_EXE = shutil.which('gcc')
ML_EXE = shutil.which('ocamlopt.byte')


def run_proc(*args, **kwargs):
  """Runs a process, dumping output if it fails."""
  sys.stdout.flush()

  proc = subprocess.Popen(
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      *args,
      **kwargs
  )
  stdout, stderr = proc.communicate()
  if proc.returncode != 0:
    print("\n%s: exited with %d" % (' '.join(args[0]), proc.returncode))
    print("\nstdout:\n%s" % stdout.decode('utf-8'))
    print("\nstderr:\n%s" % stderr.decode('utf-8'))
    sys.exit(-1)


def build_executable(obj_path, exe_path):
  if platform.system() == 'Linux':
    run_proc([MUSL_GCC_EXE, obj_path, '-o', exe_path, '-lc', '-no-pie'])
    return
  if platform.system() == 'Darwin':
    run_proc([HOST_GCC_EXE, obj_path, '-o', exe_path])
    return
  raise Error('Unsupported platform')


def run_asm_test(path, output_dir):
  """Runs an assembly test."""

  genm_lnk = os.path.join(output_dir, 'out.S')
  run_proc([OPT_EXE, path, '-o', genm_lnk])


def run_c_test(path, output_dir):
  """Runs a C test."""

  genm_src = os.path.join(output_dir, 'test.o')
  genm_lnk = os.path.join(output_dir, 'test.genm')
  genm_opt = os.path.join(output_dir, 'test.opt.genm')
  genm_asm = os.path.join(output_dir, 'test.S')
  genm_obj = os.path.join(output_dir, 'test.opt.o')
  genm_exe = os.path.join(output_dir, 'test')

  # Build an executable, passing the source file through genm.
  run_proc([
      GENM_GCC_EXE,
      path,
      '-O2',
      '-fno-stack-protector',
      '-fomit-frame-pointer',
      '-o', genm_src
  ])
  run_proc([LINK_EXE, genm_src, '-o', genm_lnk])
  run_proc([OPT_EXE, genm_lnk, '-o', genm_opt, '-O2'])
  run_proc([OPT_EXE, genm_lnk, '-o', genm_asm, '-O2'])
  run_proc([OPT_EXE, genm_lnk, '-o', genm_obj, '-O2'])
  build_executable(genm_obj, genm_exe)

  # Run the executable.
  run_proc([genm_exe])


def run_ml_test(path, output_dir):
  """Runs a ML test."""

  src = os.path.dirname(path)
  name, _ = os.path.splitext(os.path.basename(path))
  ext_path = os.path.join(src, name + '_ext.c')

  ml_src = os.path.join(output_dir, name + '.ml')
  c_src = os.path.join(output_dir, name + '_ext.c')

  genm_lnk = os.path.join(output_dir, 'test.genm')
  genm_obj = os.path.join(output_dir, 'test.opt.o')
  genm_src = os.path.join(output_dir, 'test.opt.S')
  genm_exe = os.path.join(output_dir, 'test')

  shutil.copyfile(path, ml_src)
  shutil.copyfile(ext_path, c_src)

  # Compile a bundle from all ML stuff.
  run_proc(
      [
          ML_EXE,
          '-O2',
          ml_src,
          c_src,
          '-o', genm_lnk
      ],
      cwd=output_dir
  )

  # Generate an executable.
  run_proc([OPT_EXE, genm_lnk, '-o', genm_obj, '-O2'])
  run_proc([OPT_EXE, genm_lnk, '-o', genm_src, '-O2'])
  build_executable(genm_obj, genm_exe)

  # Run the executable.
  run_proc([genm_exe])


def run_test(path, output_dir=None):
  """Runs a test, detecting type from the extension."""

  print(path)

  def run_in_directory(output_dir):
    """Runs a test in a temporary directory."""
    if path.endswith('.S'):
      return run_asm_test(path, output_dir)
    if path.endswith('.c'):
      return run_c_test(path, output_dir)
    if path.endswith('.ml'):
      return run_ml_test(path, output_dir)
    raise Exception('Invalid path type')

  if output_dir:
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    run_in_directory(output_dir)
  else:
    with tempfile.TemporaryDirectory() as root:
      run_in_directory(root)


def run_tests(tests):
  """Runs a test suite."""
  for test in tests:
    run_test(os.path.abspath(test))



if __name__ == '__main__':
  if sys.argv[1:]:
    assert len(sys.argv) == 3
    # Run all tests specified in command line arguments.
    run_test(os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]))
  else:
    # Run all tests in the test directory.
    def find_tests():
      for directory, _, files in os.walk(os.path.join(PROJECT, 'test')):
        for file in files:
          if not file.endswith('_ext.c'):
            yield os.path.join(directory, file)

    run_tests(find_tests())
