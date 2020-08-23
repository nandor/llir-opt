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
OPT_EXE = os.path.join(
    PROJECT,
    'Debug' if IS_DEBUG else 'Release',
    'tools',
    'llir-opt',
    'llir-opt'
)
OPT_LEVEL = '-O3'


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


def _process(line):
  return ' '.join(w for w in line.replace('\t', ' ').split(' ') if w)


def run_asm_test(path, output_dir):
  """Runs an assembly test."""

  args = []
  checks = []
  with open(path, 'r') as f:
    for line in f.readlines():
      line = line.strip()
      if line.startswith('# ARGS:'):
        for arg in ' '.join(line.split('# ARGS:')[1:]).split(' '):
          arg = arg.strip()
          if arg:
            args.append(arg)
      if line.startswith('# CHECK:'):
        checks.append(' '.join(line.split('# CHECK:')[1:]).strip())

  llir_lnk = os.path.join(output_dir, 'out.S')
  run_proc([OPT_EXE, path, '-o', llir_lnk] + args)

  if checks:
    with open(llir_lnk, 'r') as f:
      lines = f.readlines()
    checked = 0
    for check in checks:
      while checked < len(lines) and check not in _process(lines[checked]):
        checked += 1

      if checked >= len(lines):
        print('FAIL: {} not found'.format(check))
        sys.exit(-1)



def run_test(path, output_dir=None):
  """Runs a test, detecting type from the extension."""

  print(path)

  def run_in_directory(output_dir):
    """Runs a test in a temporary directory."""
    if path.endswith('.S'):
      return run_asm_test(path, output_dir)
    raise Exception('Unknown extension: %s' % path)

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
        for file in sorted(files):
          if not file.endswith('_ext.c'):
            yield os.path.join(directory, file)

    run_tests(find_tests())
