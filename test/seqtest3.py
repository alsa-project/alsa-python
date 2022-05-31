#! /usr/bin/python
# Sample code for pyalsa Sequencer binding
# by Aldrin Martoq <amartoq@dcc.uchile.cl>
# version 2008012401 (UTC: 1201153962 secs since the epoch)
#
# This code is in the public domain,
# use it as base for creating your pyalsa
# sequencer application.

import sys
sys.path.insert(0, '..')

from pyalsa.alsaseq import *
import time
from alsamemdebug import debuginit, debug, debugdone

debuginit()

def findmidiport():
    for connections in seq.connection_list():
        cname, cid, ports = connections
        # skip midi through
        if cname == 'Midi Through':
            continue
        for port in ports:
            pname, pid, pconns = port
            pinfo = seq.get_port_info(pid, cid)
            type = pinfo['type']
            caps = pinfo['capability']
            if type & SEQ_PORT_TYPE_MIDI_GENERIC and caps & (SEQ_PORT_CAP_WRITE):
                print("Using port: %s:%s" % (cname, pname))
                return (cid, pid)
    print("No midi port found -- install timidity or other software synth for testing!")
    sys.exit(0)

# create sequencer
seq = Sequencer()

# find midi port
cid, pid = findmidiport()

# create a queue
queue = seq.create_queue()
seq.start_queue(queue)
tempo, ppq = seq.queue_tempo(queue)
print("tempo: %d ppq: %d" % (tempo, ppq))

# play notes: DO RE MI FA SOL LA
notes = [0x40, 0x42, 0x44, 0x45, 0x47, 0x49]
event = SeqEvent(SEQ_EVENT_NOTE)
for note in notes:
    event.dest = (cid, pid)
    event.queue = queue
    event.time += ppq
    event.set_data({'note.note' : note, 'note.velocity' : 64, 'note.duration' : ppq , 'note.off_velocity' : 64})
    print('event: %s %s' % (event, event.get_data()))
    seq.output_event(event)
    seq.drain_output()
    seq.sync_output_queue()

# stop queue and delete queue
seq.stop_queue(queue)
seq.delete_queue(queue)

debug([seq])

# close sequencer
del seq


debugdone()
