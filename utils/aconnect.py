#! /usr/bin/python

# aconnect.py -- python port of aconnect
# Copyright (C) 2008 Aldrin Martoq <amartoq@dcc.uchile.cl>
#
# Based on code from aconnect.c from ALSA project
# Copyright (C) 1999 Takashi Iwai
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as
#  published by the Free Software Foundation.
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
from alsaseq import *

LIST_INPUT=1
LIST_OUTPUT=2

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
                              clientname = 'aconnect.py',
                              streams = SEQ_OPEN_DUPLEX,
                              mode = SEQ_BLOCK)
        return sequencer
    except SequencerError, e:
        fatal("open sequencer: %e", e)


def usage():
    print  \
        "aconnect - python ALSA sequencer connection manager\n" \
        "Copyright (C) 1999-2000 Takashi Iwai\n" \
        "Copyright (C) 2008 Aldrin Martoq <amartoq@dcc.uchile.cl>\n" \
        "Usage: \n" \
        " * Connection/disconnection between ports\n" \
        "   %s [-options] sender receiver\n" \
        "     sender, receiver = client:port pair\n" \
        "     -d,--disconnect     disconnect\n" \
        "     -e,--exclusive      exclusive connection\n" \
        "     -r,--real #         convert real-time-stamp on queue\n" \
        "     -t,--tick #         convert tick-time-stamp on queue\n" \
        " * List connected ports (no subscription action)\n" \
        "   %s -i|-o [-options]\n" \
        "     -i,--input          list input (readable) ports\n" \
        "     -o,--output         list output (writable) ports\n" \
        "     -l,--list           list current connections of each port\n" \
        " * Remove all exported connections\n" \
        "     -x,--removeall" \
        % (sys.argv[0], sys.argv[0])

def do_list_subs(conn, text):
    l = []
    for c, p, i in conn:
        s = ""
        if i['exclusive']:
            s+= "[ex]"
        if i['time_update']:
            if i['time_real']:
                s += "[%s:%s]" % ("real", i['queue'])
            else:
                s += "[%s:%s]" % ("tick", i['queue'])
        s = "%d:%d%s" % (c, p, s)
        l.append(s)
    if len(l):
        print "\t%s: %s" % (text, ', '.join(l))


def do_list_ports(sequencer, list_perm, list_subs):
    connectionlist = sequencer.connection_list()
    for connections in connectionlist:
        clientname, clientid, connectedports = connections
        clientinfo = sequencer.get_client_info(clientid)
        type = clientinfo['type']
        count = []
        if type == SEQ_USER_CLIENT:
            type = 'user'
        else:
            type = 'kernel'
        for port in connectedports:
            portname, portid, portconns = port
            portinfo = sequencer.get_port_info(portid, clientid)
            caps = portinfo['capability']
            if not (caps & SEQ_PORT_CAP_NO_EXPORT):
                reads = SEQ_PORT_CAP_READ | SEQ_PORT_CAP_SUBS_READ
                write = SEQ_PORT_CAP_WRITE | SEQ_PORT_CAP_SUBS_WRITE
                if (list_perm & LIST_INPUT) and (caps & reads == reads):
                    count.append(port)
                elif (list_perm & LIST_OUTPUT) and (caps & write == write):
                    count.append(port)
        if len(count) > 0:
            print "client %d: '%s' [type=%s]" % (clientid, clientname, type)
            for port in count:
                portname, portid, portconns = port
                print "  %3d '%-16s'" % (portid, portname)
                if list_subs:
                    readconn, writeconn = portconns
                    do_list_subs(readconn, "Connecting To")
                    do_list_subs(writeconn, "Connected From")

def do_unsubscribe(sequencer, args):
    sender = sequencer.parse_address(args[0])
    dest = sequencer.parse_address(args[1])
    try:
        sequencer.get_connect_info(sender, dest)
    except SequencerError:
        print 'No subscription is found'
        return
    try:
        sequencer.disconnect_ports(sender, dest)
    except SequencerError, e:
        fatal("Failed to disconnect ports %s->%s - %s", sender, dest, e)

def do_subscribe(sequencer, args, queue, exclusive, convert_time, convert_real):
    sender = sequencer.parse_address(args[0])
    dest = sequencer.parse_address(args[1])
    try:
        sequencer.get_connect_info(sender, dest)
        print "Connection is already subscribed"
        return
    except SequencerError:
        pass
    sequencer.connect_ports(sender, dest, queue, exclusive, convert_time, convert_real)

def main():
    sequencer = init_seq()
    command = 'subscribe'
    list_perm = 0
    list_subs = 0
    exclusive = 0
    convert_time = 0
    convert_real = 0
    queue = 0

    try:
        opts, args = getopt.getopt(sys.argv[1:], "dior:t:elx", ["disconnect", "input", "output", "real=", "tick=", "exclusive", "list", "removeall"])
    except getopt.GetoptError:
        usage()
        sys.exit(2)
    for o, a in opts:
        if o in ("-d", "--disconnect"):
            command = 'unsubscribe'
        elif o in ("-i", "--input"):
            command = 'list'
            list_perm |= LIST_INPUT
        elif o in ("-o", "--output"):
            command = 'list'
            list_perm |= LIST_OUTPUT
        elif o in ("-e", "--exclusive"):
            exclusive = 1
        elif o in ("-r", "--real"):
            queue = int(a)
            convert_time = 1
            convert_real = 1
        elif o in ("-t", "--tick"):
            queue = int(a)
            convert_time = 1
            convert_real = 0
        elif o in ("-l", "--list"):
            list_subs = 1
        elif o in ("-x", "--removeall"):
            command = 'removeall'

    if command == 'list':
        do_list_ports(sequencer, list_perm, list_subs)
        return

    if len(args) != 2:
        usage()
        sys.exit(2)

    if command == 'unsubscribe':
        do_unsubscribe(sequencer, args)
    elif command == 'subscribe':
        do_subscribe(sequencer, args, queue, exclusive, convert_time, convert_real)


if __name__ == '__main__':
    main()
