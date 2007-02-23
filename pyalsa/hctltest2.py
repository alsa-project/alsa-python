#! /usr/bin/python
# -*- Python -*-

import select
import alsahcontrol

def parse_event_mask(events):
	if events == 0:
		return 'None'
	if events == alsahcontrol.EventMaskRemove:
		return 'Removed'
	s = ''
	for i in alsahcontrol.EventMask.keys():
		if events & alsahcontrol.EventMask[i]:
			s += '%s ' % i
	return s[:-1]

def event_callback(element, events):

	print 'CALLBACK (DEF)! [%s] %s:%i' % (parse_event_mask(events), element.name, element.index)


class MyElementEvent:

	def callback(self, element, events):
		print 'CALLBACK (CLASS)! [%s] %s:%i' % (parse_event_mask(events), element.name, element.index)

hctl = alsahcontrol.HControl()
list = hctl.list()
element1 = alsahcontrol.Element(hctl, list[0][1:])
element1.setCallback(event_callback)
element2 = alsahcontrol.Element(hctl, list[1][1:])
element2.setCallback(MyElementEvent())
print 'Watching (DEF): %s,%i' % (element1.name, element1.index)
print 'Watching (CLASS): %s,%i' % (element2.name, element2.index)
poller = select.poll()
hctl.registerPoll(poller)
while True:
	poller.poll()
	print 'Poll OK!'
	hctl.handleEvents()