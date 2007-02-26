#! /usr/bin/python
# -*- Python -*-

import os
import sys
from distutils.core import setup, Extension

setup(
	name='pyalsa',
        version='1.0.14rc3',
        author="The ALSA Team",
        author_email='alsa-devel@alsa-project.org',
        ext_modules=[
          Extension('pyalsa.alsacard',
		    ['pyalsa/alsacard.c'],
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
	],
        packages=['pyalsa'],
        scripts=[]
)

uname = os.uname()
a = 'build/lib.%s-%s-%s' % (uname[0].lower(), uname[4], sys.version[:3])
for f in ['alsacard.so', 'alsahcontrol.so', 'alsamixer.so']:
        if not os.path.exists('pyalsa/%s' % f):
                a = '../build/lib.%s-%s-%s/pyalsa/%s' % (uname[0].lower(),
                        uname[4], sys.version[:3], f)
                print a, f
                os.symlink(a, 'pyalsa/%s' % f)
