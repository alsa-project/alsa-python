#! /usr/bin/python
# -*- Python -*-

import sys
sys.path.insert(0, '../pyalsa')
del sys
import alsacontrol

ctl = alsacontrol.Control()
print 'Card info:', ctl.card_info()
try:
  print 'Hwdep devices:', ctl.hwdep_devices()
except IOError, msg:
  print 'No hwdep devices:', msg
try:
  print 'PCM devices:', ctl.pcm_devices()
except IOError, msg:
  print 'No PCM devices:', msg
try:
  print 'Rawmidi devices:', ctl.rawmidi_devices()
except IOError, msg:
  print 'No rawmidi devices:', msg
