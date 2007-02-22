#! /usr/bin/python
# -*- Python -*-

import alsamixer

def print_elem(e):
	direction = ["Playback", "Capture"]

	print "Mixer Element '%s:%i':" % (e.name, e.index)
	print '  isActive: %s' % e.isActive
	print '  isEnumerated: %s' % e.isEnumerated
	print '  hasCommonVolume: %s' % e.hasCommonVolume
	print '  hasCommonSwitch: %s' % e.hasCommonSwitch
	print '  hasCaptureSwitchExclusive: %s' % e.hasCaptureSwitchExclusive
	if e.hasSwitch(True):
		print '  getCaptureGroup: %s' % e.getCaptureGroup
	for capture in [False, True]:
		print '  is%sMono: %s' % (direction[capture], e.isMono(capture))
		print '  has%sVolume: %s' % (direction[capture], e.hasVolume(capture))
		if e.hasVolume(capture):
			print '  get%sVolumeRange: %s' % (direction[capture], e.getVolumeRange(capture))
			print '  get%sVolumeRange_dB: %s' % (direction[capture], e.getVolumeRange_dB(capture))
			print '  get%sVolumeTuple: %s' % (direction[capture], e.getVolumeTuple(capture)) 
		print '  has%sSwitch: %s' % (direction[capture], e.hasSwitch(capture))
		if e.hasSwitch(capture):
			print '  get%sSwitchTuple: %s' % (direction[capture], e.getSwitchTuple(capture)) 
		for channel in range(alsamixer.ChannelId['Last']+1):
			if e.hasChannel(channel, capture):
				print  '  has%sChannel%s: %s' % (direction[capture], channel, alsamixer.ChannelName[channel])

print 'ChannelId:'
print alsamixer.ChannelId

print 'ChannelName:'
print alsamixer.ChannelName

print 'RegoptAbstracts:'
print alsamixer.RegoptAbstract

print 'EventMask:'
print alsamixer.EventMask

print 'EventMaskRemove:', alsamixer.EventMaskRemove

mixer = alsamixer.Mixer()
mixer.attach()
mixer.load()
print 'Element Count:', mixer.count
print 'Elements:'
print mixer.list()
element = alsamixer.Element(mixer, "PCM")
element.setVolumeTuple((128, 128))
print_elem(element)
print_elem(alsamixer.Element(mixer, "Off-hook"))
del mixer
