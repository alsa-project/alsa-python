#! /usr/bin/python
# -*- Python -*-

from pyalsa.alsahcontrol import HControl, Element, Info

hctl = HControl(name='hw:1')
list = hctl.list()
for id in list:
	elem = Element(hctl, id[1:])
	info = Info(elem)
	if info.is_user:
		print 'Removing element %s' % repr(id)
		hctl.element_remove(id[1:])
