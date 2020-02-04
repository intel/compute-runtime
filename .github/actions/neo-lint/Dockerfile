#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

FROM docker.io/ubuntu:20.04

RUN apt-get -y update ; apt-get install -y --allow-unauthenticated --no-install-recommends gpg software-properties-common
RUN apt-get -y update ; apt-get install -y --allow-unauthenticated \
    cmake \
    g++-7 \
    g++-8 \
    g++-9 \
    g++-10 \
    clang-format-11 \
    git \
    pkg-config \
    ninja-build \
    file

COPY lint.sh /lint.sh

ENTRYPOINT ["/lint.sh"]
