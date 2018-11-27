#!/usr/bin/env python2

import os
import subprocess
import tempfile

PROJECT = os.path.dirname(os.path.abspath(__file__))
OPT_EXE = os.path.join(PROJECT, 'build', 'genm')
ASM_DIR = 'test/asm'

for file in os.listdir(os.path.join(PROJECT, ASM_DIR)):
  test_path = os.path.abspath(os.path.join(PROJECT, ASM_DIR, file))
  print test_path
  with tempfile.NamedTemporaryFile(suffix='.S') as tf:
    subprocess.check_call([OPT_EXE, test_path, '-o', tf.name])
