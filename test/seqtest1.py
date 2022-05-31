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
del sys
from alsamemdebug import debuginit, debug, debugdone
from pyalsa import alsaseq


def dump_portinfo(dict):
    def dl(dict, key):
        if key in dict:
            return "'" + str(dict[key]) + "'"
        return "N/A"
    def da(dict, key, search):
        tmp = None
        if key in dict:
            for k in search:
                if k & dict[key]:
                    if tmp == None:
                        tmp = str(search[k])
                    else:
                        tmp += " " + str(search[k])
            if tmp == None:
                tmp = "UNKNOWN(%d)" % (dict[key])
        else:
            tmp = "N/A"
        return tmp

    return "name=%s capability=%s type=%s" % (dl(dict, 'name'), da(dict, 'capability', alsaseq._dportcap), da(dict, 'type', alsaseq._dporttype))

def dump_list(connections, simple=True):
    for clientports in connections:
        clientname, clientid, portlist = clientports
        print("    client: %3d    %s" % (clientid, clientname))
        if not simple:
            clientinfo = sequencer.get_client_info(clientid)
            print("\t[%s]" % clientinfo)
        else:
            print()
        for port in portlist:
            portname, portid, connections = port
            print("      port: %3d:%-2d +-%s" % (clientid, portid, portname))
            if not simple:
                portinfo = sequencer.get_port_info(portid, clientid)
                print("\t[%s]" % (dump_portinfo(portinfo)))
            else:
                print()
            readconn, writeconn = connections
            for c,p,i in readconn:
                if not simple:
                    print("        connection   to: %d:%d %s" % (c,p, i))
                else:
                    print("        connection   to: %d:%d" % (c,p))
            for c,p,i in writeconn:
                if not simple:
                    print("        connection   to: %d:%d %s" % (c,p, i))
                else:
                    print("        connection   to: %d:%d" % (c,p))


debuginit()

print("01:Creating Sequencer ==============================")
sequencer = alsaseq.Sequencer()
# other examples:
# sequencer = alsaseq.Sequencer("hw", "myfancyapplication", alsaseq.SEQ_OPEN_INPUT, alsaseq.SEQ_BLOCK)
# sequencer = alsaseq.Sequencer(clientname='python-test', streams=alsaseq.SEQ_OPEN_OUTPUT)
print("    sequencer:  %s" % sequencer)
print("    name:       %s" % sequencer.name)
print("    clientname: %s" % sequencer.clientname)
print("    streams:    %d (%s)" % (sequencer.streams, str(sequencer.streams)))
print("    mode:       %d (%s)" % (sequencer.mode, str(sequencer.mode)))
print("    client_id:  %s" % sequencer.client_id)
print()

print("02:Changing some parameters ========================")
sequencer.clientname = 'pepito'
sequencer.mode = alsaseq.SEQ_BLOCK
print("    clientname: %s" % sequencer.clientname)
print("    mode:       %d (%s)" % (sequencer.mode, str(sequencer.mode)))
print()

print("03:Creating simple port ============================")
port_id = sequencer.create_simple_port("myport", alsaseq.SEQ_PORT_TYPE_APPLICATION,alsaseq.SEQ_PORT_CAP_WRITE | alsaseq.SEQ_PORT_CAP_SUBS_WRITE)
print("    port_id:    %s" % port_id)
print()

print("04:Getting port info ===============================")
port_info = sequencer.get_port_info(port_id)
print("      -->       %s" % dump_portinfo(port_info))
print("      -->       %s" % port_info)
print()

print("05:Retrieving clients and connections (as list) ====")
connections = sequencer.connection_list()
print("  %s" % (connections))
print()

print("06:Retrieving clients and connections (detailed) ===")
connections = sequencer.connection_list()
dump_list(connections, False)
print()

print("07:Connecting 'arbitrary' ports... =================")
source = (alsaseq.SEQ_CLIENT_SYSTEM, alsaseq.SEQ_PORT_SYSTEM_ANNOUNCE)
dest = (sequencer.client_id, port_id)
print("%s ---> %s" % (str(source), str(dest)))
sequencer.connect_ports(source, dest)
print()

print("08:Retrieving clients and connections (simple) =====")
connections = sequencer.connection_list()
dump_list(connections)
print()

print("09:Disconnecting previous 'arbitrary' port =========")
print("%s -X-> %s" % (str(source), str(dest)))
sequencer.disconnect_ports(source, dest)
print()

print("10:Retrieving clients and connections (simple) =====")
connections = sequencer.connection_list()
dump_list(connections)
print()

print("11:Listing known streams constants =================")
print("%s" % alsaseq._dstreams.values())
print()

print("12:Listing known mode constants ====================")
print("%s" % alsaseq._dmode.values())
print()

print("13:Listing known queue constants ===================")
print("%s" % alsaseq._dqueue.values())
print()

print("14:Listing known client type constants =============")
print("%s" % alsaseq._dclienttype.values())
print()

print("15:Listing known port caps constants ===============")
print("%s" % alsaseq._dportcap.values())
print()

print("16:Listing known port types constants ==============")
print("%s" % alsaseq._dporttype.values())
print()

print("17:Listing known event type constants ==============")
print("%s" % alsaseq._deventtype.values())
print()

print("18:Listing known event timestamp constants =========")
print("%s" % alsaseq._deventtimestamp.values())
print()

print("19:Listing known event timemode constants ==========")
print("%s" % alsaseq._deventtimemode.values())
print()

print("20:Listing known client addresses constants ========")
print("%s" % alsaseq._dclient.values())
print()

print("21:Listing known port addresses constants ==========")
print("%s" % alsaseq._dport.values())
print()

print("22:SeqEvent repr ===================================")
print("%s" % alsaseq.SeqEvent(alsaseq.SEQ_EVENT_NOTEON))
print()


print("98:Removing sequencer ==============================")
debug([sequencer])
del sequencer
print()

print("99:Listing sequencer (using new one) ===============")
dump_list(alsaseq.Sequencer().connection_list())
print()

debugdone()
print("seqtest1.py done.")
