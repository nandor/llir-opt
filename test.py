#!/usr/bin/env python3

import argparse
import os
import platform
import shutil
import subprocess
import sys
import tempfile

PROJECT = os.path.dirname(os.path.abspath(__file__))


def which(exe):
  """Finds an executable in the path."""

  if exe.startswith('/'):
    return exe

  for path in os.environ["PATH"].split(os.pathsep):
    path = os.path.join(path, exe)
    if os.path.isfile(path) and os.access(path, os.X_OK):
      return path

  print('{} not found'.format(exe))
  sys.exit(-1)


def get_opt_exe():
  """Returns the path to the opt binary."""

  if 'Debug' in os.getcwd():
    return os.path.join(PROJECT, 'Debug', 'tools', 'llir-opt', 'llir-opt')
  if 'Release' in os.getcwd():
    return os.path.join(PROJECT, 'Release', 'tools', 'llir-opt', 'llir-opt')
  return os.path.join(PROJECT, 'tools', 'llir-opt', 'llir-opt')

OPT_EXE = get_opt_exe()
CLANG_EXE = which('clang')



class RunError(Exception):
  pass


def _process(line):
  return ' '.join(w for w in line.replace('\t', ' ').split(' ') if w)


def run_opt_test(path, output_dir, comment):
  """Runs an assembly test."""

  # Open the file and parse the special lines, extracting the commands
  # to run and the strings to identify in sequence in the file.
  run_line = None
  checks = []
  with open(path, 'r') as f:
    for source_line in f.readlines():
      line = ' '.join(source_line.strip().split())
      if not line.startswith(comment):
        continue
      line = line[len(comment):].strip()
      if not ':' in line:
        continue
      cmd = line.split(':')[0].strip()
      args = ':'.join(line.split(':')[1:]).strip()
      if cmd == 'RUN':
        run_line = args
        continue
      if cmd == 'CHECK':
        checks.append(args)
        continue
      if cmd == 'DISABLED':
        return True
      raise RunError(f'Invalid check line: {source_line}')

  if run_line is None:
    raise RunError(f'Missing run command: {path}')

  run_line = run_line.replace('%opt', OPT_EXE)
  run_line = run_line.replace('%clang', CLANG_EXE)

  # Set up a pipeline to execute all commands.
  commands = run_line.split('|')

  with open(path, 'r') as f:
    procs = []

    stdin = f
    for command in commands:
      command = command.strip()
      args = command.split(' ')
      exe = args[0]
      args = [exe] + args[1:]
      try:
        proc = subprocess.Popen(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=stdin
        )
        stdin = proc.stdout
        procs.append((command, proc))
      except OSError as e:
        raise RunError('Cannot launch {}: {}'.format(exe, e))

    all_stderr = b''
    stdout, stderr = procs[-1][1].communicate()
    for command, proc in procs[:-1]:
      all_stderr += proc.stderr.read()
      code = proc.wait()
      if code != 0:
        raise RunError('Command {} failed: {}'.format(command, code))
    all_stderr += stderr

  if all_stderr:
    print('FAIL: {}'.format(all_stderr.decode('utf-8')))
    return False

  if checks:
    lines = stdout.decode('utf-8').split('\n')
    checked = 0
    for check in checks:
      while checked < len(lines) and check not in _process(lines[checked]):
        checked += 1

      if checked >= len(lines):
        print('FAIL: {} not found ({})'.format(check, ' '.join(args)))
        return False

  return True


def run_test(path, output_dir=None):
  """Runs a test, detecting type from the extension."""

  def run_in_directory(output_dir):
    """Runs a test in a temporary directory."""
    if path.endswith('.S'):
      return run_opt_test(path, output_dir, '#')
    if path.endswith('.c'):
      return run_opt_test(path, output_dir, '//')
    raise Exception(f'Unknown extension: {path}')

  print(path)

  try:
    if output_dir:
      if not os.path.exists(output_dir):
        os.makedirs(output_dir)
      return run_in_directory(output_dir)
    else:
      with tempfile.TemporaryDirectory() as root:
        return run_in_directory(root)
  except RunError as e:
    print(e)
    return False


def run_tests(tests):
  """Runs a test suite."""
  for test in tests:
    if not run_test(os.path.abspath(test)):
      return False
  return True


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

    if not run_tests(find_tests()):
      sys.exit(-1)
