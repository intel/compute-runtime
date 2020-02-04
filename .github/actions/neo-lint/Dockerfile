#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

FROM docker.io/ubuntu:20.04

RUN apt-get -y update ; apt-get install -y --no-install-recommends gpg software-properties-common
RUN apt-get -y update ; apt-get install -y --no-install-recommends clang-format-11 git

COPY lint.sh /lint.sh

ENTRYPOINT ["/lint.sh"]
