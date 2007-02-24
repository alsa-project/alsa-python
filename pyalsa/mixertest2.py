#! /usr/bin/python
# -*- Python -*-

import alsamixer
import select

def parse_event_mask(events):
	if events == 0:
		return 'None'
	if events == alsamixer.EventMaskRemove:
		return 'Removed'
	s = ''
	for i in alsamixer.EventMask.keys():
		if events & alsamixer.EventMask[i]:
			s += '%s ' % i
	return s[:-1]

def event_callback(element, events):

	print 'CALLBACK (DEF)! [%s] %s:%i' % (parse_event_mask(events), element.name, element.index)
	print '  ', element.getVolumeTuple(), element.getSwitchTuple()


class MyElementEvent:

	def callback(self, element, events):
		print 'CALLBACK (CLASS)! [%s] %s:%i' % (parse_event_mask(events), element.name, element.index)
		print '  ', element.getVolumeTuple(), element.getSwitchTuple()


mixer = alsamixer.Mixer()
mixer.attach()
mixer.load()

element1 = alsamixer.Element(mixer, "Front")
element1.setCallback(event_callback)
element2 = alsamixer.Element(mixer, "PCM")
element2.setCallback(MyElementEvent())

poller = select.poll()
mixer.registerPoll(poller)
while True:
	poller.poll()
	print 'Poll OK!'
	mixer.handleEvents()
