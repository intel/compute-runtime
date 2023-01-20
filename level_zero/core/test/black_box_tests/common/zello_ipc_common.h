/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <cstring>
#include <sys/socket.h>

int sendmsgForIpcHandle(int socket, int fd, char *payload);
int recvmsgForIpcHandle(int socket, char *payload);
