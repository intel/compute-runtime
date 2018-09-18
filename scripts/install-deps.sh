#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
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
echo "deb http://ppa.launchpad.net/intel-opencl/intel-opencl/ubuntu xenial main" >> /etc/apt/sources.list
apt-key adv --keyserver keyserver.ubuntu.com --recv-keys B9732172C4830B8F

apt-get -y update
if [ $? -ne 0 ]
then
	wait_apt
	apt-get -y update
fi

apt-get install -y --allow-unauthenticated cmake ninja-build intel-igc-opencl-dev
if [ $? -ne 0 ]
then
	wait_apt
	apt-get install -y --allow-unauthenticated cmake ninja-build intel-igc-opencl-dev
fi

dpkg -r ccache

