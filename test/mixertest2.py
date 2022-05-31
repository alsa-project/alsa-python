#! /usr/bin/python
# -*- Python -*-

import sys
sys.path.insert(0, '../pyalsa')
del sys
from pyalsa import alsamixer
import select

def parse_event_mask(events):
	if events == 0:
		return 'None'
	if events == alsamixer.event_mask_remove:
		return 'Removed'
	s = ''
	for i in alsamixer.event_mask.keys():
		if events & alsamixer.event_mask[i]:
			s += '%s ' % i
	return s[:-1]

def event_callback(element, events):

	print('CALLBACK (DEF)! [%s] %s:%i' % (parse_event_mask(events), element.name, element.index))
	print('  ', element.get_volume_tuple(), element.get_switch_tuple())


class MyElementEvent:

	def callback(self, element, events):
		print('CALLBACK (CLASS)! [%s] %s:%i' % (parse_event_mask(events), element.name, element.index))
		print('  ', element.get_volume_tuple(), element.get_switch_tuple())


mixer = alsamixer.Mixer()
mixer.attach()
mixer.load()

element1 = alsamixer.Element(mixer, "Front")
element1.set_callback(event_callback)
element2 = alsamixer.Element(mixer, "PCM")
element2.set_callback(MyElementEvent())

poller = select.poll()
mixer.register_poll(poller)
while True:
	poller.poll()
	print('Poll OK!')
	mixer.handle_events()
