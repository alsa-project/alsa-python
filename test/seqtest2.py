#! /usr/bin/python
# Sample code for pyalsa Sequencer binding
# by Aldrin Martoq <amartoq@dcc.uchile.cl>
# version 2008012401 (UTC: 1201153962 secs since the epoch)
#
# This code is in the public domain,
# use it as base for creating your pyalsa
# sequencer application.

import sys
sys.path.insert(0, '../pyalsa')

import alsaseq
import time
from alsamemdebug import debuginit, debug, debugdone

debuginit()

seq = alsaseq.Sequencer()

def dump(event):
    print "event: %s" % event
    print "  ",
    for attr in alsaseq.SeqEvent.__dict__:
        if attr.startswith('is_'):
            t = event.__getattribute__(attr)
            if t:
                print "%s" % attr,
    print
    data = event.get_data()
    print "  data=%s" % data

print "sequencer: %s" % seq

port_id = seq.create_simple_port('hola', alsaseq.SEQ_PORT_TYPE_APPLICATION,
                                 alsaseq.SEQ_PORT_CAP_SUBS_READ | alsaseq.SEQ_PORT_CAP_READ | alsaseq.SEQ_PORT_CAP_WRITE | alsaseq.SEQ_PORT_CAP_SUBS_WRITE
                                 )

print "portid: %d" % port_id

c=-2
wait = 5000

while True:
    if c == -1:
        src = (alsaseq.SEQ_CLIENT_SYSTEM,alsaseq.SEQ_PORT_SYSTEM_ANNOUNCE)
        dest = (seq.client_id, port_id)
        print 'connecting %s -> %s' % (src, dest)
        seq.connect_ports(src, dest)
    if c == 5:
        break
    print 'waiting %s...' % wait
    events = seq.receive_events(wait)
    for event in events:
        c = 0
        dump(event)
        del event
    c += 1

debug([seq])

debugdone()
