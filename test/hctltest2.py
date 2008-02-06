#! /usr/bin/python
# -*- Python -*-

import sys
sys.path.insert(0, '../pyalsa')
del sys
import select
import alsahcontrol

def parse_event_mask(events):
	if events == 0:
		return 'None'
	if events == alsahcontrol.event_mask_remove:
		return 'Removed'
	s = ''
	for i in alsahcontrol.event_mask.keys():
		if events & alsahcontrol.event_mask[i]:
			s += '%s ' % i
	return s[:-1]

def event_callback(element, events):

	info = alsahcontrol.Info(element)
	value = alsahcontrol.Value(element)
	print 'CALLBACK (DEF)! [%s] %s:%i' % (parse_event_mask(events), element.name, element.index)
	print '  ', value.get_tuple(info.type, info.count)


class MyElementEvent:

	def callback(self, element, events):
		info = alsahcontrol.Info(element)
		value = alsahcontrol.Value(element)
		print 'CALLBACK (CLASS)! [%s] %s:%i' % (parse_event_mask(events), element.name, element.index)
		print '  ', value.get_tuple(info.type, info.count)

hctl = alsahcontrol.HControl(mode=alsahcontrol.open_mode['NONBLOCK'])
list = hctl.list()
element1 = alsahcontrol.Element(hctl, list[0][1:])
element1.set_callback(event_callback)
element2 = alsahcontrol.Element(hctl, list[1][1:])
element2.set_callback(MyElementEvent())
nelementid = (alsahcontrol.interface_id['MIXER'], 0, 0, "Z Test Volume", 1)
try:
	hctl.element_remove(nelementid)
except IOError:
	pass
hctl.element_new(alsahcontrol.element_type['INTEGER'],
		nelementid, 2, 0, 100, 1)
hctl.element_unlock(nelementid)
# handleEvents() must be here to update internal alsa-lib's element list
hctl.handle_events()
element3 = alsahcontrol.Element(hctl, nelementid)
element3.set_callback(event_callback)
print 'Watching (DEF): %s,%i' % (element1.name, element1.index)
print 'Watching (CLASS): %s,%i' % (element2.name, element2.index)
print 'Watching (DEF): %s,%i' % (element3.name, element3.index)
poller = select.poll()
hctl.register_poll(poller)
while True:
	poller.poll()
	print 'Poll OK!'
	hctl.handle_events()
