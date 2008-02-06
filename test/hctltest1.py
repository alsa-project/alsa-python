#! /usr/bin/python
# -*- Python -*-

import sys
sys.path.insert(0, '../pyalsa')
del sys
import alsahcontrol

def info(element):
	info = alsahcontrol.Info(element)
	enumerated = alsahcontrol.element_type['ENUMERATED']
	integer = alsahcontrol.element_type['INTEGER']
	integer64 = alsahcontrol.element_type['INTEGER64']
	for a in dir(info):
		if a.startswith('__'):
			continue
		if a in ['items', 'item_names'] and info.type != enumerated:
			continue
		if a in ['min', 'max', 'step'] and info.type != integer:
			continue
		if a in ['min64', 'max64', 'step64'] and info.type != integer64:
			continue
		extra = ''
		if a == 'type':
			extra = ' (%s)' % alsahcontrol.element_type_name[info.type]
		print '  %s: %s%s' % (a, getattr(info, a), extra)

def value(element):
	info = alsahcontrol.Info(element)
	value = alsahcontrol.Value(element)
	for a in dir(value):
		if a.startswith('__'):
			continue
		print '  %s: %s' % (a, getattr(value, a))
	values = value.get_tuple(info.type, info.count)
	print '  Values: ', values
	value.set_tuple(info.type, values)
	value.read()
	if info.is_writable:
		value.write()

print 'interface_id:'
print '  ', alsahcontrol.interface_id
print 'interface_name:'
print '  ', alsahcontrol.interface_name
print 'element_type:'
print '  ', alsahcontrol.element_type
print 'element_type_name:'
print '  ', alsahcontrol.element_type_name
print 'event_class:'
print '  ', alsahcontrol.event_class
print 'event_mask:'
print '  ', alsahcontrol.event_mask
print 'event_mask_remove:', alsahcontrol.event_mask_remove
print '  ', alsahcontrol.open_mode
print 'event_mask_remove:', alsahcontrol.open_mode

hctl = alsahcontrol.HControl()
print 'Count: ', hctl.count
list = hctl.list()
print 'List:'
print list
for l in list:
	print '*****'
	element1 = alsahcontrol.Element(hctl, l[1:])
	info(element1)
	print '-----'
	value(element1)
del hctl
