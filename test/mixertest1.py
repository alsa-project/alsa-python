#!/usr/bin/env python3
# -*- Python -*-

import sys
sys.path.insert(0, '../pyalsa')
del sys
from alsamemdebug import debuginit, debug, debugdone
import alsamixer

def print_elem(e):
	direction = ["playback", "capture"]

	print("Mixer Element '%s:%i':" % (e.name, e.index))
	print('  is_active: %s' % e.is_active)
	print('  is_enumerated: %s' % e.is_enumerated)
	print('  has_common_volume: %s' % e.has_common_volume)
	print('  has_common_switch: %s' % e.has_common_switch)
	print('  has_capture_switch_exclusive: %s' % e.has_capture_switch_exclusive)
	if e.has_switch(True):
		print('  getCaptureGroup: %s' % e.get_capture_group)
	for capture in [False, True]:
		print('  is_%s_mono: %s' % (direction[capture], e.is_mono(capture)))
		print('  has_%s_volume: %s' % (direction[capture], e.has_volume(capture)))
		if e.has_volume(capture):
			print('  get_%s_volume_range: %s' % (direction[capture], e.get_volume_range(capture)))
			print('  get_%s_volume_range_dB: %s' % (direction[capture], e.get_volume_range_dB(capture)))
			print('  get_%s_volume_tuple: %s' % (direction[capture], e.get_volume_tuple(capture)))
			print('  get_%s_volume_array: %s' % (direction[capture], e.get_volume_array(capture)))
			vrange = e.get_volume_range(capture)
			if vrange:
				for vol in range(vrange[0], vrange[1]+1):
					print('    vol %i == %idB' % (vol, e.ask_volume_dB(vol, capture)))
			dbrange = e.get_volume_range_dB(capture)
			if dbrange:
				for vol in range(dbrange[0], dbrange[1]+1):
					print('    dBvol %i == %i' % (vol, e.ask_dB_volume(vol, -1, capture)))
		print('  has_%s_switch: %s' % (direction[capture], e.has_switch(capture)))
		if e.has_switch(capture):
			print('  get_%s_switch_tuple: %s' % (direction[capture], e.get_switch_tuple(capture)))
		for channel in range(alsamixer.channel_id['LAST']+1):
			if e.has_channel(channel, capture):
				print( '  has_%s_channel%s: %s' % (direction[capture], channel, alsamixer.channel_name[channel]))

debuginit()

print('channel_id:')
print(alsamixer.channel_id)

print('channel_name:')
print(alsamixer.channel_name)

print('regopt_abstracts:')
print(alsamixer.regopt_abstract)

print('event_mask:')
print(alsamixer.event_mask)

print('event_mask_remove:', alsamixer.event_mask_remove)

mixer = alsamixer.Mixer()
mixer.attach()
mixer.load()
print('Element Count:', mixer.count)
print('Elements:')
print(mixer.list())
element = alsamixer.Element(mixer, "PCM")
element.set_volume_array([128, 128])
print_elem(element)
element.set_volume_tuple([127, 127])
print_elem(element)
print_elem(alsamixer.Element(mixer, "Off-hook"))

debug([element])
del element
debug([mixer])
del mixer

debugdone()
