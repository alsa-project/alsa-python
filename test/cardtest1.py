#!/usr/bin/env python3
# -*- Python -*-

import sys
sys.path.insert(0, '..')
del sys
from pyalsa import alsacard

print('asoundlibVersion:', alsacard.asoundlib_version())
print('cardLoad:', alsacard.card_load(0))
print('cardList:', alsacard.card_list())
print('deviceNameHint for all cards:')
print(alsacard.device_name_hint(-1, "pcm"))
for card in alsacard.card_list():
	print('deviceNameHint for card #%i:' % card)
	print(alsacard.device_name_hint(card, "pcm"))
