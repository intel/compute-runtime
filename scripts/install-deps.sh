#!/bin/bash
#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

wait_apt() {
	while fuser -u -v /var/lib/dpkg/lock
	do
		echo wait
		sleep 5
	done
}
echo "deb http://ppa.launchpad.net/ocl-dev/intel-opencl/ubuntu xenial main" >> /etc/apt/sources.list
apt-key adv --keyserver keyserver.ubuntu.com --recv-keys C3086B78CC05B8B1

apt-get -y update
if [ $? -ne 0 ]
then
	wait_apt
	apt-get -y update
fi

apt-get --no-install-recommends install -y --allow-unauthenticated cmake ninja-build libigc-dev intel-gmmlib-dev
if [ $? -ne 0 ]
then
	wait_apt
	apt-get --no-install-recommends install -y --allow-unauthenticated cmake ninja-build libigc-dev intel-gmmlib-dev
fi

dpkg -r ccache

