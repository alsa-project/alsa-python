#! /usr/bin/python
# -*- Python -*-

import os
import sys
try:
	from setuptools import setup, Extension
except ImportError:
	from distutils.core import setup, Extension

VERSION='1.0.25'

if os.path.exists("version"):
	fp = open("version", "r")
	ver = fp.readline()
	fp.close()
	ver = ver[:-1]
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

uname = os.uname()
a = 'build/lib.%s-%s-%s' % (uname[0].lower(), uname[4], sys.version[:3])
for f in ['alsacard.so', 'alsacontrol.so', 'alsahcontrol.so', 'alsamixer.so', 'alsaseq.so']:
        if not os.path.exists('pyalsa/%s' % f):
                a = '../build/lib.%s-%s-%s/pyalsa/%s' % (uname[0].lower(),
                        uname[4], sys.version[:3], f)
                print a, f
                os.symlink(a, 'pyalsa/%s' % f)
