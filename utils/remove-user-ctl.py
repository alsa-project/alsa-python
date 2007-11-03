#! /usr/bin/python
# -*- Python -*-

from pyalsa.alsahcontrol import HControl, Element, Info

hctl = HControl()
list = hctl.list()
for id in list:
	elem = Element(hctl, id[1:])
	info = Info(elem)
	if info.isUser:
		print 'Removing element %s' % repr(id)
		hctl.elementRemove(id[1:])
