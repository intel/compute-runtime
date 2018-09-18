#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

pacman -Sy --noconfirm clang cmake git make wget pkg-config fakeroot ninja sudo \
       perl-io-string perl-test-pod autoconf automake patch
useradd -m build -g wheel
sed -i "s/^# %wheel ALL=(ALL) NOPASSWD: ALL/%wheel ALL=(ALL) NOPASSWD: ALL/" /etc/sudoers
cp -a /root/*.sh /home/build
su -l build /home/build/build-arch-dep.sh

