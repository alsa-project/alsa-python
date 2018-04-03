#!/usr/bin/env python3
# -*- Python -*-

import os
import sys
import stat
try:
  from setuptools import setup, Extension
except ImportError:
  from distutils.core import setup, Extension

VERSION='1.0.29'

if os.path.exists("version"):
  fp = open("version", "r")
  ver = fp.readline()[:-1]
  fp.close()
else:
  ver = None
if ver != VERSION:
  fp = open("version", "w+")
  fp.write(VERSION + '\n')
  fp.close()
del fp

setup(
  name='pyalsa',
  version=VERSION,
  description="Python binding for the ALSA library.",
  url="http://www.alsa-project.org",
  download_url="ftp://ftp.alsa-project.org/pub/pyalsa/",
  licence="LGPLv2+",
  author="The ALSA Team",
  author_email='alsa-devel@alsa-project.org',
  ext_modules=[
    Extension('pyalsa.alsacard',
      ['pyalsa/alsacard.c'],
      include_dirs=[],
      library_dirs=[],
      libraries=['asound']),
    Extension('pyalsa.alsacontrol',
      ['pyalsa/alsacontrol.c'],
      include_dirs=[],
      library_dirs=[],
      libraries=['asound']),
    Extension('pyalsa.alsahcontrol',
      ['pyalsa/alsahcontrol.c'],
      include_dirs=[],
      library_dirs=[],
      libraries=['asound']),
    Extension('pyalsa.alsamixer',
      ['pyalsa/alsamixer.c'],
      include_dirs=[],
      library_dirs=[],
      libraries=['asound']),
    Extension('pyalsa.alsaseq',
      ['pyalsa/alsaseq.c'],
      include_dirs=[],
      library_dirs=[],
      libraries=['asound']),
  ],
  packages=['pyalsa'],
  scripts=[]
)

SOFILES = [
  'alsacard',
  'alsacontrol',
  'alsahcontrol',
  'alsamixer',
  'alsaseq'
]

uname = os.uname()
dir = 'build/lib.%s-%s-%s/pyalsa' % (uname[0].lower(), uname[4], sys.version[:3])
files = os.path.exists(dir) and os.listdir(dir) or []
for f in SOFILES:
  path = ''
  for f2 in files:
    if f2.startswith(f + '.') and f2.endswith('.so'):
      path = dir + '/' + f2
      break
  if not path or not os.path.exists(path):
    continue
  p = 'pyalsa/%s.so' % f
  print("%s -> %s" % (p, path))
  try:
    st = os.lstat(p)
    if stat.S_ISLNK(st.st_mode):
      os.remove(p)
  except:
    pass
  os.symlink('../' + path, 'pyalsa/%s.so' % f)
