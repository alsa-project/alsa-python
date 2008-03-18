#! /usr/bin/python

# coverage.py -- python coverage of asoundlib API
# Copyright(C) 2008 by Aldrin Martoq <amartoq@dcc.uchile.cl>
#  Licensed under GPL v2 (see below).
#
# Description:
#    This tool aims to help in completing the python binding of the asoundlib
#    API. Actually, it's being coded for the current pyalsa, but it can be
#    easily modified to support other styles of codes (mainly by changing the
#    pyparsing objects used to scan the source code).
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


from APICoverage import *
import APICoverage
from pprint import pprint
import time

__author__ = "Aldrin Martoq <amartoq@dcc.uchile.cl>"
__version__ = "1.0"
__license__ = "GNU General Public License version 2"
__copyright__ = "Copyright(C) 2008 by Aldrin Martoq <amartoq@dcc.uchile.cl>"


# setup and do the work
APICoverage.source_dir = '../pyalsa'
APICoverage.api_url = 'http://www.alsa-project.org/alsa-doc/alsa-lib/'

time_start = time.time()

# common C parser
ident = Word(alphas + "_", alphanums + "_")
type_and_var = \
    Optional(oneOf("const unsigned struct")) \
    + ident + Optional(Word("*")) + ident
args = OneOrMore(type_and_var) + ZeroOrMore("," + type_and_var)
function_declaration = type_and_var + "(" + Group(args) + ")"
function_define = \
    "#define" + ident \
    + "(" + OneOrMore(ident) + ZeroOrMore("," + ident) + Optional("...") + ")"
function_define_block = \
    "#define" + ident + "{"
#    + Group(("(" + args + Optional("...") + ")") | "{")

# common asoundlib parser
snd_ = Regex("snd_[\w_]+")
SND_ = Regex("SND_[\w_]+")

# pyalsa/alsaseq.c parser
alsaseq_SEQ = Regex("SEQ_[\w_]+")
alsaseq_Constant = \
    "TCONSTADD(module," + alsaseq_SEQ + "," + quotedString + "," + alsaseq_SEQ
alsaseq_typedef = \
    Literal("typedef") \
    + "struct" \
    + "{" \
    + "PyObject_HEAD" \
    + Optional(";") \
    + OneOrMore((type_and_var + ";") | cppStyleComment) \
    + "}" \
    + ident.setName("struct_name")
alsaseq_function = \
    "static" \
    + function_declaration.setName("static_function")
alsaseq_index = function_define | function_define_block \
    | alsaseq_typedef | alsaseq_function

# pyalsa/{alsacard, alsacontrol, alsahcontrol, alsamixer}.c parser
addspace = Regex("add_space[0-9]")
addspace_def = \
    Suppress(addspace + "(") \
    + quotedString + Suppress(",") + ident + Suppress(");")
alsaall_Constant = \
    Suppress(Literal("#define") \
    + addspace \
    + "(pname, name) { \\" \
    + "o = PyInt_FromLong(") \
    + SND_ \
    + Suppress("##name); \\") \
    + Suppress("PyDict_SetItemString(d1, pname, o); \\") \
    + Suppress("Py_DECREF(o); }") \
    + OneOrMore(Group(addspace_def)) \
    + Suppress("PyDict_SetItemString(d,") \
    + quotedString



alsaall_typedef = \
    Literal("struct") \
    + "{" \
    + "PyObject_HEAD" \
    + Optional(";") \
    + OneOrMore((type_and_var + ";") | cppStyleComment) \
    + "}"
alsaall_function = \
    "static" \
    + function_declaration.setName("static_function")
alsaall_index = function_define | alsaall_typedef | alsaall_function

# read all files
pyalsaseq = read_source('alsaseq.c')
pyalsacard = read_source('alsacard.c')
pyalsacontrol = read_source('alsacontrol.c')
pyalsahcontrol = read_source('alsahcontrol.c')
pyalsamixer = read_source('alsamixer.c')

# parse all files with parser
index = get_cached_parse([
        (pyalsaseq, alsaseq_index),
        (pyalsacard, alsaall_index),
        (pyalsacontrol, alsaall_index),
        (pyalsahcontrol, alsaall_index),
        (pyalsamixer, alsaall_index)
        ], "index")
index_snd_ = get_cached_parse([
        (pyalsaseq, snd_),
        (pyalsacard, snd_),
        (pyalsacontrol, snd_),
        (pyalsahcontrol, snd_),
        (pyalsamixer, snd_)
        ], "index_snd_")
index_SND_ = get_cached_parse([
        (pyalsaseq, SND_),
        (pyalsacard, SND_),
        (pyalsacontrol, SND_),
        (pyalsahcontrol, SND_),
        (pyalsamixer, SND_)
        ], "index_SND_")
index_Constant_alsaseq = get_cached_parse([
        (pyalsaseq, alsaseq_Constant)
        ], "index_Constant_alsaseq")
index_Constant_alsaall = get_cached_parse([
        (pyalsacard, alsaall_Constant),
        (pyalsacontrol, alsaall_Constant),
        (pyalsahcontrol, alsaall_Constant),
        (pyalsamixer, alsaall_Constant)
        ], "index_Constant_alsaall")

# urls of doxygen documentation
urls = [
    # globals
    "group___global.html",
    # error handling
    "group___error.html",
    # control
    "group___control.html",
    "group___h_control.html",
    "group___s_control.html",
    # sequencer
    "group___sequencer.html",
    "group___seq_client.html",
    "group___seq_port.html",
    "group___seq_subscribe.html",
    "group___seq_queue.html",
    "group___seq_event.html",
    "group___seq_misc.html",
    "group___seq_ev_type.html",
    "group___seq_events.html",
    "group___seq_middle.html",
    "group___m_i_d_i___event.html",
    # mixer
    "group___mixer.html",
    "group___simple_mixer.html"
    ]

def look_constant(name):
    """
    Look name for constant declarations.
    
    Returns:
      a list of strings with the file and python constant.
    """
    nlist = []
    for file in index_Constant_alsaseq:
        for lc in index_Constant_alsaseq[file]:
            tokens, start, end = lc
            rs = "SND_" + tokens[5]
            if rs == name:
                nlist.append("%14s: %s" % (file, tokens[5]))
                nlist.append("%14s: %s[%s]" % 
                              (file, tokens[1].lower(), tokens[3]))
    for file in index_Constant_alsaall:
        for lc in index_Constant_alsaall[file]:
            tokens, start, end = lc
            prefix = tokens[0]
            dictname = tokens[-1].split('"')[1]
            for i in range(1, len(tokens)-1):
                rs = prefix + tokens[i][1]
                if rs == name:
                    nlist.append("%14s: %s[%s]" %
                                 (file, dictname, tokens[i][0]))

    return nlist

def look_usage(name):
    """
    Look name for usage (appareance) in C functions, structs, etc.

    Returns:
      a list of strings with the file and matched function/struct/define.
    """
    name = name.strip()
    dict = {}
    for file in index_snd_:
        list = []
        for lu in index_snd_[file]:
            tokens, start, end = lu
            rs = tokens[0]
            if rs == name:
                list.append(start)
        dict[file] = list
    for file in index_SND_:
        list = []
        for lu in index_SND_[file]:
            tokens, start, end = lu
            rs = tokens[0]
            if rs == name:
                list.append(start)
        if dict.has_key(file):
            dict[file].extend(list)
        else:
            dict[file] = list

    nlist = []
    for file in dict:
        for lstart in dict[file]:
            if not index.has_key(file):
                continue
            found = None
            previous = None
            for call in index[file]:
                tokens, start, end = call
                if start > lstart:
                    found = previous
                    break
                previous = tokens[2]
                if tokens[0] == 'typedef':
                    previous = tokens[-1]
                elif tokens[0] == '#define':
                    previous = tokens[1]
                elif previous == '*':
                    previous = tokens[3]
                #print "%8s: %5d - %20s %s" % (file, start, name, previous)
            if found != None:
                nlist.append("%14s: %s" % (file, found))
                #print "FOUND: %8s: %5d %s" % (file, lstart, found)
    return nlist


# Following string contains excluded/commented API tokens, one per line.
# Format is:
# token comment
comments = """
SND_SEQ_DLSYM_VERSION I think there is no real use in pyalsa

snd_seq_open_lconf need a snd_config port for implementing it
snd_seq_poll_descriptors_revents no real use in pyalsa
snd_seq_system_info_copy no real use in pyalsa
snd_seq_system_info_free no real use in pyalsa
snd_seq_system_info_malloc snd_seq_system_info_alloca used instead
snd_seq_system_info_sizeof currently not used

snd_seq_client_info_copy no real use in pyalsa
snd_seq_client_info_free no real use in pyalsa
snd_seq_client_info_malloc snd_seq_client_info_alloca used instead
snd_seq_client_pool_copy no real use in pyalsa
snd_seq_client_pool_free no real use in pyalsa
snd_seq_client_pool_get_client snd_seq_client_id used instead
snd_seq_client_pool_malloc no real use in pyalsa
snd_seq_client_pool_set_input_pool snd_seq_set_client_pool_input used instead
snd_seq_client_pool_set_output_pool snd_seq_set_client_pool_output used instead
snd_seq_client_pool_set_output_room snd_seq_set_client_pool_output_room used instead
snd_seq_client_pool_sizeof currently not used
snd_seq_set_client_pool snd_seq_set_client_pool_* used instead
snd_seq_client_info_set_name snd_seq_set_client_name used instead
snd_seq_client_info_sizeof currently not used

snd_seq_get_port_info snd_seq_get_any_port_info used instead
snd_seq_port_info_copy no real use in pyalsa
snd_seq_port_info_free no real use in pyalsa
snd_seq_port_info_malloc snd_seq_port_info_alloca used instead
snd_seq_port_info_set_addr snd_seq_port_info_set_client, snd_seq_port_info_set_port used instead
snd_seq_port_info_sizeof currently not used

snd_seq_port_subscribe_copy no real use in pyalsa
snd_seq_port_subscribe_free no real use in pyalsa
snd_seq_port_subscribe_malloc snd_seq_port_subscribe_alloca used instead
snd_seq_port_subscribe_sizeof currently not used
snd_seq_query_subscribe_copy no real use in pyalsa
snd_seq_query_subscribe_free no real use in pyalsa
snd_seq_query_subscribe_get_client snd_seq_query_subscribe_get_addr used instead
snd_seq_query_subscribe_get_index currently not used
#snd_seq_query_subscribe_get_num_subs 
snd_seq_query_subscribe_get_port snd_seq_query_subscribe_get_addr used instead
snd_seq_query_subscribe_get_root currently not used
snd_seq_query_subscribe_get_type currently not used
snd_seq_query_subscribe_malloc no real use in pyalsa
snd_seq_query_subscribe_set_client snd_seq_query_subscribe_set_index used instead
snd_seq_query_subscribe_set_port snd_seq_query_subscribe_set_index used instead
snd_seq_query_subscribe_sizeof currently not used
"""


print """
*******************************
PYALSA/ASOUNDLIB COVERAGE/USAGE
*******************************


Notes:
* For re-generating this file, you need inet access (for downloading the
  doxygen HTML from www.alsa-project.org site).
* Some cached information about parsing is in the 'cache' directory:
  * If you change a source file, the program will regenerate the cache.
  * HTML API from www.alsa-project.org is never refreshed, remove manually.
* Doxygen Modules are underlined by ======= .
* Doxygen Sections are Underlined by ------- . The parsed sections are:
  * CPP #defines
  * Type definitions
  * Enumerations
  * Functions
* The first line of each item is the "C Prototype". First 3 columns are:
  * Number of times found in checked code
  * N/A if the item is not being used/was not found
  * EXC if the item is excluded (will not be used/implemented)
* The next line is the one-liner documentation.
* Following lines are usage/definition until next item.
* There are lines that summaries the coverage, they start with '^STAT'.



"""


print_api_coverage(urls, look_constant, look_usage, comments)

# print end line
time_end = time.time()
time_diff = time_end - time_start

print """%s
Generated for ALSA project by alsa-python-coverage.py %s
%s UTC (%s@%s %3.3f seconds).
""" % ("-"*72,
       __version__,
       time.asctime(time.gmtime(time_start)), 
       os.getlogin(),
       os.uname()[1],
       time_diff
       )
print
