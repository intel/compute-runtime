#!/usr/bin/env python
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

# Usage:
# ./scripts/build_spec.py manifests/manifest.yml scripts/fedora.spec.in <version> <revision>
#

import datetime
import git
import re
import sys
import yaml

if len(sys.argv) < 5:
    print "ERROR! invalid number of parameters"
    print
    print "Usage:"
    print "  ./scripts/build_spec.py <manifest> <spec.in> <version> <revision>"
    print
    sys.exit(1)

manifest_file = sys.argv[1]

repo = git.Repo(".")
neo_revision = repo.head.commit

with open(manifest_file, 'r') as f:
    manifest = yaml.load(f)

gmmlib_revision = manifest['components']['gmmlib']['revision']

c = repo.commit(neo_revision)
cd = datetime.datetime.fromtimestamp(c.committed_date)

pkg_version = "%s.%s.%s" %(str(cd.isocalendar()[0])[-2:], cd.isocalendar()[1], sys.argv[3])

with open(sys.argv[2], 'r') as f:
    for line in f.readlines():
        if not re.match(".*__NEO_COMMIT_ID__$", line.strip()) is None:
            print "%s" % (line.rstrip().replace("__NEO_COMMIT_ID__", "%s" % neo_revision))
            continue

        if not re.match(".*__GMMLIB_COMMIT_ID__$", line.strip()) is None:
            print "%s" % (line.rstrip().replace("__GMMLIB_COMMIT_ID__", "%s" % gmmlib_revision))
            continue

        if not re.match(".*__NEO_PACKAGE_VERSION__$", line.strip()) is None:
            print "%s" % (line.rstrip().replace("__NEO_PACKAGE_VERSION__", "%s" % pkg_version))
            continue

        if not re.match(".*__NEO_PACKAGE_RELEASE__.*", line.strip()) is None:
            print "%s" % (line.rstrip().replace("__NEO_PACKAGE_RELEASE__", "%s" % sys.argv[4]))
            continue

        print line.rstrip()

