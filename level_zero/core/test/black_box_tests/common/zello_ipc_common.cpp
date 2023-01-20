/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_ipc_common.h"

int sendmsgForIpcHandle(int socket, int fd, char *payload) {
    char cmsgBuf[CMSG_SPACE(sizeof(ze_ipc_mem_handle_t))];

    struct iovec msgBuffer;
    msgBuffer.iov_base = payload;
    msgBuffer.iov_len = ZE_MAX_IPC_HANDLE_SIZE;

    struct msghdr msgHeader = {};
    msgHeader.msg_iov = &msgBuffer;
    msgHeader.msg_iovlen = 1;
    msgHeader.msg_control = cmsgBuf;
    msgHeader.msg_controllen = CMSG_LEN(sizeof(fd));

    struct cmsghdr *controlHeader = CMSG_FIRSTHDR(&msgHeader);
    controlHeader->cmsg_type = SCM_RIGHTS;
    controlHeader->cmsg_level = SOL_SOCKET;
    controlHeader->cmsg_len = CMSG_LEN(sizeof(fd));

    *(int *)CMSG_DATA(controlHeader) = fd;
    ssize_t bytesSent = sendmsg(socket, &msgHeader, 0);
    if (bytesSent < 0) {
        return -1;
    }

    return 0;
}

int recvmsgForIpcHandle(int socket, char *payload) {
    int fd = -1;
    char cmsgBuf[CMSG_SPACE(sizeof(ze_ipc_mem_handle_t))];

    struct iovec msgBuffer;
    msgBuffer.iov_base = payload;
    msgBuffer.iov_len = ZE_MAX_IPC_HANDLE_SIZE;

    struct msghdr msgHeader = {};
    msgHeader.msg_iov = &msgBuffer;
    msgHeader.msg_iovlen = 1;
    msgHeader.msg_control = cmsgBuf;
    msgHeader.msg_controllen = CMSG_LEN(sizeof(fd));

    ssize_t bytesSent = recvmsg(socket, &msgHeader, 0);
    if (bytesSent < 0) {
        return -1;
    }

    struct cmsghdr *controlHeader = CMSG_FIRSTHDR(&msgHeader);
    memmove(&fd, CMSG_DATA(controlHeader), sizeof(int)); // NOLINT(clang-analyzer-core.NonNullParamChecker)
    return fd;
}