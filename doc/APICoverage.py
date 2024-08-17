#! /usr/bin/python

# APICoverage.py -- helper for API coverage tools
# Copyright(C) 2008 by Aldrin Martoq <amartoq@dcc.uchile.cl>
#  Licensed under GPL v2 (see below).
#
# Description:
#    This file provides the base for creating an API coverage tool. It may
#    be used in other projects, for an example see the coverage.py tool.
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
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA


import os, pickle, urllib.request, urllib.parse, urllib.error, sys
from pyparsing import *
from html.entities import entitydefs
from htmllib import HTMLParser
from formatter import AbstractFormatter, DumbWriter

# cache dir (preparsed source and HTML asoundlib API)
cache = 'cache'
# directory of source code
source_dir = '.'
# base url
api_url = ''
# summary (section) counter
summary_total = summary_miss = summary_exc = 0
# subsection (defines, typedefs, etc) counter
count_total = count_miss = count_exc = 0

if not os.path.exists(cache):
    print("Creating cache dir: %s" % cache)
    os.makedirs(cache)

def read_source(name):
    """ Reads the specified source file """
    filename = os.path.join(source_dir, name)
    return (name, "".join(open(filename).readlines()), filename)

def list_to_str(alist):
    tmp = []
    for i in alist:
        if type(i) is ParseResults:
            tmp.append(list_to_str(i))
        else:
            tmp.append(str(i))
    return tmp
            

def get_cached_parse(index_parser_list, name):
    """
    Generate the scan lists from the parsers, store it in a file in the
    cachedir and return it; if the file already exists it just return it
    without processing.
    """
    name = os.path.join(cache, name)

    if os.path.exists(name):
        modified = False
        for source, parser in index_parser_list:
            if os.stat(source[2]).st_mtime > os.stat(name).st_mtime:
                modified = True
        if not modified:
            return pickle.load(open(name, "rb"))

    print("generating cache, file: %s" % name, end=' ')
    dict = {}
    for source, parser in index_parser_list:
        print(source[0], end=' ')
        sys.stdout.flush()
        list = []
        for tokenlist, start, end in parser.scanString(source[1]):
            tlist = list_to_str(tokenlist)
            list.append((tlist, int(start), int(end)))
        dict[source[0]] = list
    pickle.dump(dict, open(name, "wb"))
    print()
    return dict

# API html parsing/caching

def get_cached_api(url, name):
    """
    Download the HTML API from the specified url and stores it in a
    file in the cachedir; if the file already exists it just returns it
    contents.
    """
    name = os.path.join(cache, name)
    if os.path.exists(name):
        data = "".join(open(name).readlines())
    else:
        print("downloading %s" % url)
        # The ALSA project API web pages use UTF-8
        data = str(urllib.request.urlopen(url).read().decode('utf-8'))
        open(name, "w").write(data)
    return data



class AsoundlibAPIHTMLParser(HTMLParser):
    """
    Customized HTMLParser, it adds some markers for easy parsing the
    HTML asoundlib API from the alsa website.
    """
    
    HTMLParser.entitydefs['nbsp'] = ' '
    
    def __init__(self, name, data):
        f = AbstractFormatter(DumbWriter(open(name, 'w'), 100))
        HTMLParser.__init__(self, f)
        self.feed(data)
        self.close()

    def start_h1(self, attrs):
        HTMLParser.start_h1(self, attrs)
        self.handle_data("--- titlestart")
        self.do_br(None)

    def start_table(self, attrs):
        if len(attrs) == 1 and attrs[0] == ("class", "memname"):
            self.handle_data("--- itemstart")
            self.do_br(None)

    def start_tr(self, attrs):
        self.do_br(None)

    def anchor_end(self):
        pass

def parse_asoundlib_api(lines):
    """
    Parses an html file (given as a set of lines including '\n').
    Returns a list of: title, defines, typedefs, enums, functions.
    """
    state = 0
    defines = []
    typedefs = []
    enums = []
    functions = []
    current = None
    title = None
    name = ""
    comment = ""
    enumsublist = []
    for line in lines:
        line = line[:-1]
        if False:
            if id(current) == id(defines):
                print("defines   ", end=' ')
            elif id(current) == id(typedefs):
                print("typedefs  ", end=' ')
            elif id(current) == id(enums):
                print("enums     ", end=' ')
            elif id(current) == id(functions):
                print("functions ", end=' ')
            else:
                print("          ", end=' ')
            print("%s %d %s" % (id(current), state, line))

        if line.startswith('Define Documentation'):
            current = defines
            state = 0
        elif line.startswith('Typedef Documentation'):
            current = typedefs
            state = 0
        elif line.startswith('Enumeration Type Documentation'):
            current = enums
            state = 0
        elif line.startswith('Function Documentation'):
            current = functions
            state = 0
        elif line.startswith('--- itemstart'):
            state = 1
        elif line.startswith('--- titlestart'):
            state = 5
        elif state == 5:
            title = line
            state = 0
        elif current == None:
            continue
        elif state == 1:
            if line == "":
                state = 2
            else:
                name += line
        elif state == 2:
            if id(current) == id(enums):
                state = 3
            else:
                comment = line
                current.append((name, comment))
                name = ""
                comment = ""
                state = 0
        elif state == 3 and line.startswith('Enumerator:'):
            state = 4
            enumsublist = []
        elif state == 4:
            if line == "":
                current.append((name, comment, enumsublist))
                name = ""
                comment = ""
                state = 0
            else:
                enum, subcomment = line.split(' ', 1)
                enumsublist.append((enum, subcomment))

    return (title, defines, typedefs, enums, functions)
        

def print_name(d0, d1, name, look_constant, look_usage, exclude_list):
    """ 
    Prints a defined entry (typedef, function, etc) and its usage to stdout.
    It also updates counters ({summary,count}_{total,miss,exc}).

    Parameters:
      d0 -- entry prototype
      d1 -- entry documentation (one liner)
      name -- entry name (define, function name, etc)
      look_constant -- lookup name for constant (a defined python function)
      look_usage -- lookup name for usage (a defined python function)
    """
    global summary_total, summary_miss, summary_exc
    global count_total, count_miss, count_exc
    summary_total += 1
    count_total += 1
    lc = look_constant(name)
    uc = look_usage(name)
    usecount = len(lc) + len(uc)
    exccount = 0
    for token, comment in exclude_list:
        if token == name:
            exccount += 1
    if usecount == 0:
        if exccount > 0:
            used = "EXC"
            summary_exc += 1
            count_exc += 1
        else:
            used = "N/A"
            summary_miss += 1
            count_miss += 1
    else:
        used = "%s" % usecount
        
    print("%-4s%s" % (used, d0))
    print("%8s%s" % ("", d1))

    if usecount > 0:
        excstr = "Comment"
    else:
        excstr = "Excluded"
    for token, comment in exclude_list:
        if token == name:
            print("%10s==> %11s: %s" % ("", excstr, comment))
    for s in lc:
        print("%10s=> As constant: %s" % ("", s))
    for s in uc:
        print("%10s=> Used in    : %s" % ("", s))
    if used == "N/A":
        print(" "*10 + "**** NOT AVAILABLE/USED %s ****" % name)


def _print_stat(title, section, missing, excluded, total):
    if total == 0:
        fmissing = "N/A"
        fexcluded = "N/A"
        fcovered = "N/A"
    else:
        fmissing = (100*(float(missing)/float(total)))
        fexcluded = (100*(float(excluded)/float(total)))
        fcovered = 100 - fmissing
        fmissing = "%3.0f%%" % fmissing
        fexcluded = "%3.0f%%" % fexcluded
        fcovered = "%3.0f%%" % fcovered
    print("STAT %-30.30s %-12.12s: " % (title, section) + \
        "%3d missing (%4s) %3d excluded (%4s) of %3d total (%4s covered)." % \
        (missing, fmissing, excluded, fexcluded, total, fcovered))


def print_stat(title, section):
    """
    Print STATS line for the given title and section. It will
    reset the section counters (count_{total,miss}).
    """
    global count_total, count_miss, count_exc
    _print_stat(title, section, count_miss, count_exc, count_total)
    count_total = count_miss = count_exc = 0

def print_summary_stat(title):
    """
    Print STATS line for the last given title and section. It will
    reset the title summary counters (summary_{total,miss}).
    """
    global summary_total, summary_miss, summary_exc
    _print_stat(title, "SUMMARY", summary_miss, summary_exc, summary_total)
    summary_total = summary_miss = summary_exc = 0


def parse_excludes(excludes):
    list = []
    for line in excludes.splitlines():
        s = line.split(' ', 1)
        if len(s) > 1:
            token = s[0]
            comment = s[1]
            list.append((token, comment))
    return list



def print_api_coverage(urls, look_constant, look_usage, excludes):
    
    el = parse_excludes(excludes)

    for url in urls:
        data = get_cached_api(api_url + url, url)
        tmp = os.path.join(cache, 'tmp')
        AsoundlibAPIHTMLParser(tmp, data)
        (title, defines, typedefs, enums, functions) = \
            parse_asoundlib_api(open(tmp).readlines())
        print(title)
        print("="*len(title))
        print("\n"*2)
        #print "%s\n%s\n%s\n%s\n%s\n\n" % \
        #    (title, defines, typedefs, enums, functions)
        summary_total = 0
        summary_miss = 0
        if len(defines) > 0:
            print("Defines")
            print("-------")
            for d in defines:
                name = d[0].split(' ')[1]
                print_name(d[0], d[1], name, look_constant, look_usage, el)
            print_stat(title, "Defines")
            print("\n"*2)
        if len(typedefs) > 0:
            print("Typedefs")
            print("--------")
            for d in typedefs:
                names = d[0].split(' ')
                name = names[-1]
                if ')' in name:
                    names = d[0].split('(')
                    name = names[-2].split()[-1]
                print_name(d[0], d[1], name, look_constant, look_usage, el)
            print_stat(title, "Typedefs")
            print("\n"*2)
        if len(enums) > 0:
            print("Enumerations")
            print("------------")
            for e in enums:
                print("%s" % e[0])
                for d in e[2]:
                    name = d[0]
                    print_name(d[0], d[1], name, look_constant, look_usage, el)
            print_stat(title, "Enumerations")
            print("\n"*2)
        if len(functions) > 0:
            print("Functions")
            print("---------")
            for d in functions:
                name = None
                for n in d[0].split(' '):
                    if n.startswith('snd_'):
                        name = n
                    elif n.startswith('('):
                        break
                if name != None:
                    print_name(d[0], d[1], name, look_constant, look_usage, el)
            print_stat(title, "Functions")
            print("\n"*2)
        print_summary_stat(title)
        print("\n"*4)

