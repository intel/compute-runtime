#!/bin/bash
#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

check_deb() {
    DriverVer=$(dpkg -l 'intel-opencl' | awk '/ii  intel-opencl / { print $3 }')
    if [ -z $DriverVer ]
    then
        DriverVer=$(dpkg -l 'intel-opencl-icd' | awk '/ii  intel-opencl-icd / { print $3 }')
    fi
}

check_rpm() {
    DriverVer=$(rpm -q --queryformat '%{VERSION}' intel-opencl)
    if [ $? != 0 ]
    then
        DriverVer=""
    fi
}

check_pacman() {
    DriverVer=$(pacman -Q intel-compute-runtime | awk '{print $2}' | sed "s/-.*$//")
}

if [ -f /etc/os-release ]
then
    source /etc/os-release
    if [ -z "${ID}" ]
    then
        echo "Unknown OS"
        exit 1
    fi
fi

case "${ID}" in
    debian | ubuntu ) check_deb
        ;;
    fedora | centos | rhel) check_rpm
        ;;
    arch ) check_pacman
        ;;
    * ) echo "Unsupported OS: ${ID}"
        exit 1
        ;;
esac

if [ -z $DriverVer ]
then
    echo No driver detected in the system
    exit 1
fi

if [ $# -eq 0 ]
then
    echo $DriverVer
    exit 1
fi


if [ $# -ne 1 ] || [ $1 == "-h" ] || [ $1 == "--help" ]
then
    echo $0 called with no parameters, prints the version of the installed OpenCL driver
    echo $0 called with a single parameter containing expected version number,
    echo returns success \(0\) if installed the specified driver version or newer
    echo returns failure \(1\) if no driver or older than specified
    exit 1
fi

if ! [[ $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+.* ]]; then
    echo Invalid version format
    exit 1
fi

TestedString=$(echo "$1" | awk -F. '{ printf("%d.%02d.%d\n", $1,$2,$3); }';)

DriverStatus=$( echo -e "${DriverVer}\n${TestedString}"  | sort -V -C -r ; echo $? )

if [ $DriverStatus -eq 1 ]
then
    echo Driver $DriverVer is older than referenced version passed from command line ${TestedString}
else
    echo Driver $DriverVer is newer than or equal to referenced version passed from command line ${TestedString}
fi

exit $DriverStatus

