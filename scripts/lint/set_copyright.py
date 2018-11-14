#!/usr/bin/env python
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

import re
import sys
import os.path
from datetime import date
from stat import ST_MODE

# count arguments, need at least 1
if len(sys.argv) < 2:
    print "need 1 argument, file\n"
    sys.exit(0)

header_cpp = """/*
 * Copyright (C) %s Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */
"""

header_bash_style = """#
# Copyright (C) %s Intel Corporation
#
# SPDX-License-Identifier: MIT
#
"""

allowed_extensions = [
    'cpp', 'h', 'inl', 'hpp', 'm',
    'cmake',
    'py', 'sh',
    'cl',
    'exports'
]

allowed_extensions_2 = [
    'h.in',
    'options.txt'
]

allowed_files = [
    'CMakeLists.txt'
]

banned_paths = [
    'scripts/tests/copyright/in',
    'scripts/tests/copyright/out',
    'third_party'
]

cpp_sharp_lines = [
    '#pragma',
    '#include'
]

def isBanned(path):
    path_ok = False
    for banned_path in banned_paths:
        if dirname.startswith(banned_path):
            path_ok = True
            break
    return path_ok

for path in sys.argv:

    # avoid self scan
    if os.path.abspath(path) == os.path.abspath(sys.argv[0]):
        continue

    # check whether we should scan this file
    path_ext = path.split('.')
    path_ok = False
    filename = os.path.basename(path)
    dirname = os.path.dirname(path)
    while True:
        if isBanned(path):
            path_ok = False
            break

        if filename in allowed_files:
            path_ok = True
            break

        if path_ext[-1].lower() in allowed_extensions:
            path_ok = True
            break

        if '.'.join(path_ext[-2:]) in allowed_extensions_2:
            path_ok = True
            break
        break

    if not path_ok:
        print "[MIT] Ignoring file: %s" % path
        continue

    # check that first arg is a existing file
    if not os.path.isfile(path):
        print "cannot find file %s, skipping" % path
        continue

    print "[MIT] Processing file: %s" % path

    L = list()
    start_year = None
    header = header_cpp
    header_start = '/*'
    header_end = '*/'
    comment_char = "\*"

    # now read line by line
    f = open(path, 'r')

    # take care of hashbang
    first_line = f.readline()
    if not first_line.startswith( '#!' ):
        line = first_line
        first_line = ''
    else:
        line = f.readline()

    # check whether comment type is '#'
    try:
        if first_line or line.startswith('#'):
            for a in cpp_sharp_lines:
                print "a: %s ~ %s" % (a, line)
                if line.startswith(a):
                    raise "c++"
            header_start = '#'
            header_end = '\n'
            header = header_bash_style
            comment_char = "#"
    except:
        pass

    curr_comment = list()

    # copyright have to be first comment in file
    if line.startswith(header_start):
        isHeader = True
        isHeaderEnd = False
    else:
        isHeader = False
        isHeaderEnd = True

    isCopyright = False

    while (line):
        if isHeader:
            if header_end == '\n' and len(line.strip()) == 0:
                isHeader = False
                isHeaderEnd = True
            elif line.strip().endswith(header_end):
                isHeader = False
                isHeaderEnd = True
            elif "Copyright" in line:
                tmp = line.split(',')
                expr  = ("^%s Copyright \([Cc]\) (\d+)( *- *\d+)?" % comment_char)
                m = re.match(expr, line.strip())
                if not m is None:
                    start_year = m.groups()[0]
                    curr_comment = list()
                    isCopyright = True
            if not isCopyright:
                curr_comment.append(line)
        elif isCopyright and isHeaderEnd:
            if len(line.strip()) > 0:
                L.append(line)
                isHeaderEnd = False
        else:
            L.append(line)

        line = f.readline()

    f.close()

    year = date.today().year
    if start_year is None:
        start_year = str(year)
    elif int(start_year) < year:
        start_year += "-"
        start_year += str(year)

    # store file mode because we want to preserve this
    old_mode = os.stat(path)[ST_MODE]

    os.remove(path)
    f = open(path, 'w')

    if first_line:
        f.write(first_line)

    f.write(header % start_year)

    if len(curr_comment)>0 or len(L)>0:
        f.write("\n")

    if len(curr_comment)>0:
        f.write(''.join(curr_comment))

    contents = ''.join(L)
    f.write(contents)
    f.close()

    # chmod to original value
    os.chmod(path, old_mode)

    del L[:]
