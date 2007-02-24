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

	info = alsahcontrol.Info(element)
	value = alsahcontrol.Value(element)
	print 'CALLBACK (DEF)! [%s] %s:%i' % (parse_event_mask(events), element.name, element.index)
	print '  ', value.getTuple(info.type, info.count)


class MyElementEvent:

	def callback(self, element, events):
		info = alsahcontrol.Info(element)
		value = alsahcontrol.Value(element)
		print 'CALLBACK (CLASS)! [%s] %s:%i' % (parse_event_mask(events), element.name, element.index)
		print '  ', value.getTuple(info.type, info.count)

hctl = alsahcontrol.HControl(mode=alsahcontrol.OpenMode['NonBlock'])
list = hctl.list()
element1 = alsahcontrol.Element(hctl, list[0][1:])
element1.setCallback(event_callback)
element2 = alsahcontrol.Element(hctl, list[1][1:])
element2.setCallback(MyElementEvent())
nelementid = (alsahcontrol.InterfaceId['Mixer'], 0, 0, "Z Test Volume", 1)
try:
	hctl.elementRemove(nelementid)
except IOError:
	pass
hctl.elementNew(alsahcontrol.ElementType['Integer'],
		nelementid, 2, 0, 100, 1)
hctl.elementUnlock(nelementid)
# handleEvents() must be here to update internal alsa-lib's element list
hctl.handleEvents()
element3 = alsahcontrol.Element(hctl, nelementid)
element3.setCallback(event_callback)
print 'Watching (DEF): %s,%i' % (element1.name, element1.index)
print 'Watching (CLASS): %s,%i' % (element2.name, element2.index)
print 'Watching (DEF): %s,%i' % (element3.name, element3.index)
poller = select.poll()
hctl.registerPoll(poller)
while True:
	poller.poll()
	print 'Poll OK!'
	hctl.handleEvents()
