#! /usr/bin/python
# -*- Python -*-

import alsahcontrol

print 'InterfaceId:'
print '  ', alsahcontrol.InterfaceId
print 'InterfaceName:'
print '  ', alsahcontrol.InterfaceName
print 'ElementType:'
print '  ', alsahcontrol.ElementType
print 'ElementTypeName:'
print '  ', alsahcontrol.ElementTypeName
print 'EventClass:'
print '  ', alsahcontrol.EventClass
print 'EventMask:'
print '  ', alsahcontrol.EventMask
print 'EventMaskRemove:', alsahcontrol.EventMaskRemove
print '  ', alsahcontrol.OpenMode
print 'EventMaskRemove:', alsahcontrol.OpenMode

hctl = alsahcontrol.HControl()
print 'Count: ', hctl.count
list = hctl.list()
print 'List:'
print list
element1 = alsahcontrol.Element(hctl, list[0][1:])
del hctl
