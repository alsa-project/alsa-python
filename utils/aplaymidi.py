#! /usr/bin/python

# aplaymidi.py -- python port of aplaymidi
# Copyright (C) 2008 Aldrin Martoq <amartoq@dcc.uchile.cl>
#
# Based on code from aplaymidi.c from ALSA project
# Copyright (C) 2004-2006 Clemens Ladisch <clemens@ladisch.de>
#
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA


import sys
sys.path.insert(0, '../pyalsa')

import getopt
import os
import struct
import alsaseq
import traceback
import time
from alsaseq import *

class SMFError(Exception):
    """ Exception raised for errors while reading a SMF file """
    pass


def errormsg(msg, *args):
    """ prints an error message to stderr """
    sys.stderr.write(msg % args)
    sys.stderr.write('\n')
    traceback.print_exc(file=sys.stderr)


def fatal(msg, *args):
    """ prints an error message to stderr, and dies """
    errormsg(msg, *args)
    sys.exit(1)


def init_seq():
    """ opens an alsa sequencer """
    try:
        sequencer = Sequencer(name = 'default',
                              clientname = 'aplaymidi.py',
                              streams = SEQ_OPEN_DUPLEX,
                              mode = SEQ_BLOCK)
        return sequencer
    except SequencerError, e:
        fatal("open sequencer: %e", e)


def parse_ports(sequencer, portspec):
    """ parses one or more port addresses from the string, separated by ','
    example: 14:0,Timidity
    """
    portlist = []
    if portspec == None:
        return portlist
    ports = portspec.split(',')
    for port in ports:
        try:
            client, port = sequencer.parse_address(port)
            portlist.append((client, port))
        except SequencerError, e:
            fatal("Failed to parse port %s - %s", port, e)
    return portlist


def create_source_port(sequencer):
    try :
        port = sequencer.create_simple_port(name = 'aplaymidi.py',
                                            type = SEQ_PORT_TYPE_MIDI_GENERIC \
                                                | SEQ_PORT_TYPE_APPLICATION,
                                            caps = SEQ_PORT_CAP_NONE)
        return port
    except SequencerError, e:
        fatal("Failed to create port - %s", e)


def create_queue(sequencer):
    try:
        queue = sequencer.create_queue(name = 'aplaymidi')
        return queue
    except SequencerError, e:
        fatal("Failed to create queue - %s", e)


def connect_ports(sequencer, port_id, ports):
    client_id = sequencer.client_id
    for client, port in ports:
        sequencer.connect_ports((client_id, port_id), (client, port))


def read_byte(file):
    """ read a single byte
    raises SMFError if end of file was detected
    """
    s = file.read(1)
    if s == '':
        raise SMFError('End of file reached at position %d' % file.tell())
    return struct.unpack('B', s)[0]


def read_32_le(file):
    """ reads a little-endian 32-bit integer """
    value = 0
    for p in (0, 8, 16, 24):
        c = read_byte(file)
        value |= c << p
    return value


def read_id(file):
    """ reads a 4-char identifier """
    return file.read(4)


def read_int(file, bytes):
    """ reads a fixed-size big-endian number """
    value = 0
    while bytes > 0:
        c = read_byte(file)
        value = (value << 8) | c
        bytes -= 1
    return value


def read_var(file):
    """ reads a variable-length number """
    c = read_byte(file)
    value = c & 0x7f
    if c & 0x80:
        c = read_byte(file)
        value = (value << 7) | (c & 0x7f)
        if c & 0x80:
            c = read_byte(file)
            value = (value << 7) | (c & 0x7f)
            if c & 0x80:
                c = read_byte(file)
                value = (value << 7) | (c & 0x7f)
                if c & 0x80:
                    raise SMFError('Invalid variable-length number at file position %d' % file.tell())
    return value

def skip(file, bytes):
    """ skip the specified number of bytes """
    file.read(bytes)


def read_track(file, chunk_len, ports, smtpe_timing):
    """ reads one complete track from the file """

    tick = 0
    last_cmd = 0
    port = 0
    typemap = {}
    typemap[0x8] = SEQ_EVENT_NOTEOFF
    typemap[0x9] = SEQ_EVENT_NOTEON
    typemap[0xa] = SEQ_EVENT_KEYPRESS
    typemap[0xb] = SEQ_EVENT_CONTROLLER
    typemap[0xc] = SEQ_EVENT_PGMCHANGE
    typemap[0xd] = SEQ_EVENT_CHANPRESS
    typemap[0xe] = SEQ_EVENT_PITCHBEND

    eventlist = []

    track_end = file.tell() + chunk_len
    while file.tell() < track_end:
        delta_ticks = read_var(file)
        if delta_ticks < 0:
            break
        tick += delta_ticks

        c = read_byte(file)

        if c < 0:
            break

        if c & 0x80:
            cmd = c
            if cmd < 0xf0:
                last_cmd = cmd
        else:
            # running status
            cmd = last_cmd
            file.seek(file.tell() - 1)
            if not cmd:
                break


        s = cmd >> 4
        if s >= 0x8 and s <= 0xa:
            event = SeqEvent(type=typemap[s])
            event.dest = ports[port]
            event.time = tick
            event.set_data({'note.channel' : cmd & 0x0f,
                           'note.note' : read_byte(file) & 0x7f,
                           'note.velocity' : read_byte(file) & 0x7f
                           })
            eventlist.append(event)
        elif s == 0xb or s == 0xe:
            event = SeqEvent(type=typemap[s])
            event.dest = ports[port]
            event.time = tick
            event.set_data({'control.channel' : cmd & 0x0f,
                           'control.param' : read_byte(file) & 0x7f,
                           'control.value' : read_byte(file) & 0x7f
                           })
            eventlist.append(event)
        elif s == 0xc or s == 0xd:
            event = SeqEvent(type=typemap[s])
            event.dest = ports[port]
            event.time = tick
            event.set_data({'control.channel' : cmd & 0x0f,
                           'control.value' : read_byte(file) & 0x7f
                           })
            eventlist.append(event)
        elif s == 0xf:
            if cmd == 0xf0 or cmd == 0xf7:
                # sysex
                l = read_var(file)
                if l < 0:
                    break
                if cmd == 0xf0:
                    l += 1
                event = SeqEvent(type=SEQ_EVENT_SYSEX)
                event.dest = ports[port]
                event.time = tick
                sysexdata = []
                if cmd == 0xf0:
                    sysexdata.append(0xf0)
                while len(sysexdata) < l:
                    sysexdata.append(read_byte(file))
                event.set_data({'ext' : sysexdata})
                eventlist.append(event)
            elif cmd == 0xff:
                c = read_byte(file)
                l = read_var(file)
                if l < 0:
                    break
                if c == 0x21:
                    # port number
                    port = read_byte() % len(ports)
                    skip(file, l - 1)
                elif c == 0x2f:
                    # end track
                    track_endtick = tick
                    skip(file, track_end - file.tell())
                    return track_endtick, eventlist
                elif c == 0x51:
                    # tempo
                    if l < 3:
                        break
                    if smtpe_timing:
                        skip(file, l)
                    else:
                        event = SeqEvent(type=SEQ_EVENT_TEMPO)
                        event.dest = (SEQ_CLIENT_SYSTEM, SEQ_PORT_SYSTEM_TIMER)
                        event.time = tick
                        tempo = read_byte(file) << 16
                        tempo |= read_byte(file) << 8
                        tempo |= read_byte(file)
                        event.set_data({'queue.param.value' : tempo})
                        eventlist.append(event)
                        skip(file, l - 3)
                else:
                    # ignore all other meta events
                    skip(file, l)
            else:
                break
        else:
            break

    raise SMFError('Invalid MIDI data at file position (%d)' % file.tell())

def read_smf(file, ports):
    """ reads an entire MIDI file """

    tempo = 0
    ppq = 0
    tracks = []


    header_len = read_int(file, 4)
    if header_len < 6:
        raise SMFError('Invalid header_len: %d' % header_len)

    type = read_int(file, 2)
    if type != 0 and type != 1:
        raise SMFError('Not supported type: %d' % type)

    num_tracks = read_int(file, 2)
    if num_tracks < 1 or num_tracks > 1000:
        raise SMFError('Invalid number of tracks: %d' % num_tracks)

    time_division = read_int(file, 2)
    if time_division < 0:
        raise SMFError('Invalid time division: %d' % time_division)

    smtpe_timing = time_division & 0x8000

    if not smtpe_timing:
        # time division is ticks per quarter
        tempo = 500000
        ppq = time_division
    else:
        i = 0x80 - ((time_division >>8) & 0x7f)
        time_division &= 0xff
        if i == 24:
            tempo = 500000
            ppq = 12 * time_division
        elif i == 25:
            tempo = 400000
            ppq = 10 * time_division
        elif i == 29:
            tempo = 100000000
            ppq = 2997 * time_division
        elif i == 30:
            tempo = 500000
            ppq = 15 * time_division
        else:
            raise SMFError('Invalid number of SMPTE frames per second (%d)'
                           % i)

    # read tracks
    while num_tracks > 0:
        # search for MTrk chunk
        while True:
            id = read_id(file)
            chunk_len = read_int(file, 4)
            if chunk_len < 0 or chunk_len >= 0x10000000:
                raise SMFError('Invalid chunk length %d' % chunk_len)
            if id == 'MTrk':
                break
            skip(file, chunk_len)
        trackevents = read_track(file, chunk_len, ports, smtpe_timing)
        tracks.append(trackevents)
        num_tracks -= 1

    return tempo, ppq, tracks

def read_riff(file, ports):
    """ reads an entire RIFF file """

    raise NotImplementedError("Sorry, I couldn't find any RIFF midi file to implement this!  -- Aldrin Martoq")


def play(sequencer, filename, end_delay, source_port, queue, ports):
    file = sys.stdin
    ok = None
    if filename != '-':
        try:
            file = open(filename, 'rb')
        except IOError, e:
            fatal("Cannot open %s - %s", filename, e)

    id = read_id(file)
    try:
        if id == 'MThd':
            ok = read_smf(file, ports)
        elif id == 'RIFF':
            ok = read_riff(file, ports)
        else:
            raise SMFError('unrecognized id')
    except SMFError, e:
        errormsg("%s is not a Standard MIDI File - %s", filename, e)

    # now play!
    tempo, ppq, tracks = ok
    sequencer.queue_tempo(queue, tempo, ppq)
    sequencer.start_queue(queue)

    # add all events from tracks and sort them based on track, then on time
    itrack = 0
    allevents = []
    for track in tracks:
        endtick, eventlist = track
        for event in eventlist:
            d = {'itrack' : itrack, 'event' : event, 'time' : event.time}
            allevents.append(d)
        itrack += 1
    def compare(x, y):
        ret = x['time'] - y['time']
        if ret == 0:
            ret = x['itrack'] - y['itrack']
        return ret
    allevents.sort(compare)
    max_tick = allevents[-1]['time']

    for d in allevents:
        event = d['event']
        event.source = (0, 0)
        event.queue = queue
        if event.type == SEQ_EVENT_TEMPO:
            event.set_data({'queue.queue' : queue})
        sequencer.output_event(event)

    # schedule queue stop at end of song
    event = SeqEvent(SEQ_EVENT_STOP)
    event.source = (0, 0)
    event.queue = queue
    event.time = max_tick
    event.dest = (SEQ_CLIENT_SYSTEM, SEQ_PORT_SYSTEM_TIMER)
    event.set_data({'queue.queue' : queue})
    sequencer.output_event(event)

    # make sure that the sequencer sees all our events
    sequencer.drain_output()
    sequencer.sync_output_queue()

    time.sleep(end_delay)

    if file != sys.stdin:
        file.close()



def list_ports():
    sequencer = init_seq()
    print " Port    Client name                      Port name";
    clientports = sequencer.connection_list()
    for connections in clientports:
        clientname, clientid, connectedports = connections
        for port in connectedports:
            portname, portid, connections = port
            portinfo = sequencer.get_port_info(portid, clientid)
            type = portinfo['type']
            caps = portinfo['capability']
            if type & SEQ_PORT_TYPE_MIDI_GENERIC and \
                    caps & (SEQ_PORT_CAP_WRITE | SEQ_PORT_CAP_SUBS_WRITE):
                print "%3d:%-3d  %-32.32s %s" % (clientid, portid, clientname, portname)

def usage():
    print "Usage: %s -p client:port[,...] [-d delay] midifile ...\n" \
        "-h, --help                  this help\n" \
        "-V, --version               print current version\n" \
        "-l, --list                  list all possible output ports\n" \
        "-p, --port=client:port,...  set port(s) to play to\n" \
        "-d, --delay=seconds         delay after song ends\n" % (sys.argv[0])

def version():
    print "aplaymidi.py asoundlib version %s" % (SEQ_LIB_VERSION_STR)


def main():
    sequencer = init_seq()
    ports = []
    end_delay = 2

    try:
        opts, args = getopt.getopt(sys.argv[1:], "hVlp:d:", ["help", "version", "list", "port=", "delay="])
    except getopt.GetoptError:
        usage()
        sys.exit(2)
    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit(0)
        elif o in ("-V", "--version"):
            version()
            sys.exit(0)
        elif o in ("-l", "--list"):
            list_ports()
            sys.exit(0)
        elif o in ("-p", "--port"):
            ports = parse_ports(sequencer, a)
        elif o in ("-d", "--delay"):
            end_delay = int(a)

    if len(ports) < 1:
        ports = parse_ports(sequencer, os.getenv('ALSA_OUTPUT_PORTS'))
        if len(ports) < 1:
            fatal("Please specify at least one port with --port.")

    if len(args) < 1:
        fatal("Please specify a file to play.")

    source_port = create_source_port(sequencer)
    queue = create_queue(sequencer)
    connect_ports(sequencer, source_port, ports)

    for filename in args:
        play(sequencer, filename, end_delay, source_port, queue, ports)


if __name__ == '__main__':
    main()
