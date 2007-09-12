#! /usr/bin/python
# -*- Python -*-

import sys
sys.path.insert(0, '../pyalsa')
del sys
import alsacontrol

ctl = alsacontrol.Control()
print 'Card info:', ctl.cardInfo()
try:
  print 'Hwdep devices:', ctl.hwdepDevices()
except IOError, msg:
  print 'No hwdep devices:', msg
try:
  print 'PCM devices:', ctl.pcmDevices()
except IOError, msg:
  print 'No PCM devices:', msg
try:
  print 'Rawmidi devices:', ctl.rawmidiDevices()
except IOError, msg:
  print 'No rawmidi devices:', msg
