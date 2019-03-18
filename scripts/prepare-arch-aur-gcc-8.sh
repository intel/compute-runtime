#!/bin/bash
#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -e
set -x

pacman -Sy --noconfirm gcc cmake git make wget pkg-config fakeroot ninja sudo \
       perl-io-string perl-test-pod autoconf automake patch llvm clang \
       intel-gmmlib nettle gnutls p7zip python-nose python2 bison flex

useradd -m build -g wheel
sed -i "s/^# %wheel ALL=(ALL) NOPASSWD: ALL/%wheel ALL=(ALL) NOPASSWD: ALL/" /etc/sudoers
cp -a /root/*.sh /home/build
su -l build /home/build/build-arch-dep.sh

mkdir /root/build

