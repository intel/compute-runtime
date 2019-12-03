#!/usr/bin/env python3
#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

"""Usage: ./scripts/build_spec.py scripts/fedora.spec.in <version> <revision>"""

import datetime
import re
import sys
import argparse

import git


def _main():
    parser = argparse.ArgumentParser(
        description='Usage: ./scripts/build_spec.py <spec.in> <version> <revision>')
    parser.add_argument('spec')
    parser.add_argument('version')
    parser.add_argument('revision')
    args = parser.parse_args()

    repo = git.Repo('.')
    neo_revision = repo.head.commit

    neo_commit = repo.commit(neo_revision)
    commited_date = datetime.datetime.fromtimestamp(neo_commit.committed_date)

    pkg_version = '{}.{:02d}.{}'.format(
        str(commited_date.isocalendar()[0])[-2:],
        commited_date.isocalendar()[1],
        args.version)

    with open(args.spec, 'r') as fin:
        for line in fin.readlines():
            if re.match('.*__NEO_COMMIT_ID__$', line.strip()):
                print(line.rstrip().replace('__NEO_COMMIT_ID__', str(neo_revision)))

            elif re.match('.*__NEO_PACKAGE_VERSION__$', line.strip()):
                print(line.rstrip().replace(
                    '__NEO_PACKAGE_VERSION__', pkg_version))

            elif re.match('.*__NEO_PACKAGE_RELEASE__.*', line.strip()):
                print(line.rstrip().replace(
                    '__NEO_PACKAGE_RELEASE__', args.revision))

            else:
                print(line.rstrip())

    return 0


if __name__ == '__main__':
    sys.exit(_main())
