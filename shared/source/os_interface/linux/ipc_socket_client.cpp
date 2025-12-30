/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ipc_socket_client.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include <algorithm>
#include <cstddef>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace NEO {

IpcSocketClient::IpcSocketClient() = default;

IpcSocketClient::~IpcSocketClient() {
    disconnect();
}

bool IpcSocketClient::connectToServer(const std::string &socketPath) {
    if (isConnected()) {
        return true;
    }

    clientSocket = NEO::SysCalls::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (clientSocket == -1) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketClient: Failed to create socket: %s\n", strerror(errno));
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = socketTimeout / 1000;
    timeout.tv_usec = (socketTimeout % 1000) * 1000;

    NEO::SysCalls::setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    NEO::SysCalls::setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    size_t nameLen = std::min(socketPath.size(), sizeof(addr.sun_path) - 1);
    addr.sun_path[0] = '\0';
    memcpy(addr.sun_path + 1, socketPath.data(), nameLen);
    socklen_t addrLen = static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + 1 + nameLen);

    if (NEO::SysCalls::connect(clientSocket, reinterpret_cast<struct sockaddr *>(&addr), addrLen) == -1) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketClient: Failed to connect to abstract server '%s': %s\n",
                     socketPath.c_str(), strerror(errno));
        NEO::SysCalls::close(clientSocket);
        clientSocket = -1;
        return false;
    }

    serverSocketPath = socketPath;
    PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                 "IpcSocketClient: Connected to abstract server name=%s\n", socketPath.c_str());
    return true;
}

void IpcSocketClient::disconnect() {
    if (clientSocket != -1) {
        NEO::SysCalls::close(clientSocket);
        clientSocket = -1;
        serverSocketPath.clear();
    }
}

int IpcSocketClient::requestHandle(uint64_t handleId) {
    if (!isConnected()) {
        return -1;
    }

    IpcSocketMessage msg;
    msg.type = IpcSocketMessageType::requestHandle;
    msg.processId = SysCalls::getpid();
    msg.handleId = handleId;
    msg.payloadSize = 0;

    if (!sendMessage(msg)) {
        return -1;
    }

    IpcSocketResponsePayload responsePayload = {};
    int receivedFd = receiveFileDescriptor(&responsePayload, sizeof(responsePayload));

    if (receivedFd == -1) {
        IpcSocketMessage response;
        if (receiveMessage(response, &responsePayload, sizeof(responsePayload))) {
            if (response.type == IpcSocketMessageType::responseHandle &&
                response.handleId == handleId && !responsePayload.success) {
                PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                             "IpcSocketClient: Server reported failure for handle %lu\n", handleId);
            }
        }
        return -1;
    }

    if (!responsePayload.success) {
        NEO::SysCalls::close(receivedFd);
        return -1;
    }

    PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                 "IpcSocketClient: Received handle %lu as fd %d\n", handleId, receivedFd);
    return receivedFd;
}

int IpcSocketClient::receiveFileDescriptor(void *data, size_t dataSize) {
    struct msghdr msg = {};
    struct iovec iov;
    char cmsgBuffer[CMSG_SPACE(sizeof(int))];

    if (data && dataSize > 0) {
        iov.iov_base = data;
        iov.iov_len = dataSize;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
    }

    msg.msg_control = cmsgBuffer;
    msg.msg_controllen = sizeof(cmsgBuffer);

    if (NEO::SysCalls::recvmsg(clientSocket, &msg, 0) == -1) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketClient: Failed to receive message: %s\n", strerror(errno));
        return -1;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        return *reinterpret_cast<int *>(CMSG_DATA(cmsg));
    }

    return -1;
}

bool IpcSocketClient::sendMessage(const IpcSocketMessage &msg, const void *payload) {
    if (NEO::SysCalls::send(clientSocket, &msg, sizeof(msg), 0) != sizeof(msg)) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketClient: Failed to send message header: %s\n", strerror(errno));
        return false;
    }

    if (payload && msg.payloadSize > 0) {
        if (NEO::SysCalls::send(clientSocket, payload, msg.payloadSize, 0) != static_cast<ssize_t>(msg.payloadSize)) {
            PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                         "IpcSocketClient: Failed to send message payload: %s\n", strerror(errno));
            return false;
        }
    }

    return true;
}

bool IpcSocketClient::receiveMessage(IpcSocketMessage &msg, void *payload, size_t maxPayloadSize) {
    if (NEO::SysCalls::recv(clientSocket, &msg, sizeof(msg), MSG_WAITALL) != sizeof(msg)) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketClient: Failed to receive message header: %s\n", strerror(errno));
        return false;
    }

    if (payload && msg.payloadSize > 0 && maxPayloadSize >= msg.payloadSize) {
        if (NEO::SysCalls::recv(clientSocket, payload, msg.payloadSize, MSG_WAITALL) != static_cast<ssize_t>(msg.payloadSize)) {
            PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                         "IpcSocketClient: Failed to receive message payload: %s\n", strerror(errno));
            return false;
        }
    }

    return true;
}

} // namespace NEO
