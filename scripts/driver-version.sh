#!/bin/bash
#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

DriverVer=$(dpkg -l 'intel-opencl' | awk '/ii  intel-opencl / { print $3 }')

if [ -z $DriverVer ]; then
  echo No driver detected in the system
  exit 1
fi

if [ $# -eq 0 ]; then
  echo $DriverVer
  exit 1
fi

if [ $# -ne 1 ] || [ $1 == "-h" ] || [ $1 == "--help" ]; then
  echo $0 called with no parameters, prints the version of the installed OpenCL driver
  echo $0 called with a single parameter containing expected version number,
  echo returns success \(0\) if installed the specified driver version or newer
  echo returns failure \(1\) if no driver or older than specified
  exit 1
fi

DriverStatus=$(! dpkg --compare-versions "$DriverVer" "lt" "$1" ; echo $? )

if [ $DriverStatus -eq 1 ]; then
  echo Driver $DriverVer is older than referenced version passed from command line $1
else
  echo Driver $DriverVer is newer than or equal to referenced version passed from command line $1
fi

exit $DriverStatus
