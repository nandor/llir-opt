#!/usr/bin/env python2

import os
import subprocess

path = os.path.dirname(os.path.abspath(__file__))

for file in os.listdir(os.path.join(path, 'test')):
  print os.path.abspath(os.path.join(path, file))
  subprocess.check_call([
    './genm',
    os.path.abspath(os.path.join(path, 'test', file)),
    'out.s',
    '-p'
  ])
