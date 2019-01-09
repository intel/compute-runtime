#!/usr/bin/env python
#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

# Usage:
# ./scripts/build_spec.py scripts/fedora.spec.in <version> <revision>
#

import datetime
import git
import re
import sys
import yaml

if len(sys.argv) < 4:
    print "ERROR! invalid number of parameters"
    print
    print "Usage:"
    print "  ./scripts/build_spec.py <spec.in> <version> <revision>"
    print
    sys.exit(1)

repo = git.Repo(".")
neo_revision = repo.head.commit

c = repo.commit(neo_revision)
cd = datetime.datetime.fromtimestamp(c.committed_date)

pkg_version = "%s.%02d.%s" %(str(cd.isocalendar()[0])[-2:], cd.isocalendar()[1], sys.argv[2])

with open(sys.argv[1], 'r') as f:
    for line in f.readlines():
        if not re.match(".*__NEO_COMMIT_ID__$", line.strip()) is None:
            print "%s" % (line.rstrip().replace("__NEO_COMMIT_ID__", "%s" % neo_revision))
            continue

        if not re.match(".*__NEO_PACKAGE_VERSION__$", line.strip()) is None:
            print "%s" % (line.rstrip().replace("__NEO_PACKAGE_VERSION__", "%s" % pkg_version))
            continue

        if not re.match(".*__NEO_PACKAGE_RELEASE__.*", line.strip()) is None:
            print "%s" % (line.rstrip().replace("__NEO_PACKAGE_RELEASE__", "%s" % sys.argv[3]))
            continue

        print line.rstrip()

