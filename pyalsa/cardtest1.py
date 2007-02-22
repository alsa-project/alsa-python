#! /usr/bin/python
# -*- Python -*-

import alsacard

print 'asoundlibVersion:', alsacard.asoundlibVersion()
print 'cardLoad:', alsacard.cardLoad(0)
print 'cardList:', alsacard.cardList()
print 'deviceNameHint for all cards:'
print alsacard.deviceNameHint(-1, "pcm")
for card in alsacard.cardList():
	print 'deviceNameHint for card #%i:' % card
	print alsacard.deviceNameHint(card, "pcm")
