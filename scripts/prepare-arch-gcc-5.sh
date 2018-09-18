#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

pacman -Sy --noconfirm gcc54 cmake git make wget pkg-config fakeroot ninja sudo \
       perl-io-string perl-test-pod autoconf automake patch
ln -s /usr/bin/gcc-5 /usr/bin/gcc
ln -s /usr/bin/g++-5 /usr/bin/g++
useradd -m build -g wheel
sed -i "s/^# %wheel ALL=(ALL) NOPASSWD: ALL/%wheel ALL=(ALL) NOPASSWD: ALL/" /etc/sudoers
sed -i "s/ -fno-plt//g" /etc/makepkg.conf
cp -a /root/*.sh /home/build
su -l build /home/build/build-arch-dep.sh

