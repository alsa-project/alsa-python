#!/usr/bin/env python3
# -*- Python -*-

import sys
import gc

def debuginit():
	gc.set_debug(gc.DEBUG_LEAK | gc.DEBUG_STATS)
	print("GC DEBUG: init.")

def debug(what):
	from sys import getrefcount
	from gc import get_referrers
	for o in what:
		if getrefcount(o) - 3 != 1 or \
		   len(get_referrers(o)) - 2 != 1:
			print('GD DEBUG LEAK:', o, hex(id(o)), type(o), getrefcount(o)-3, len(get_referrers(o))-2)

def debugdone():
	gc.collect()
	print("GC DEBUG LEAK done.")
	None
