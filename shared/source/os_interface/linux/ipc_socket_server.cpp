/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ipc_socket_server.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <errno.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace NEO {

IpcSocketServer::IpcSocketServer() {
    auto pid = SysCalls::getpid();
    socketPath = "neo_ipc_" + std::to_string(pid);
}

IpcSocketServer::~IpcSocketServer() {
    shutdownImpl();
}

bool IpcSocketServer::initialize() {
    if (serverRunning.load()) {
        return true;
    }

    serverSocket = SysCalls::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (serverSocket == -1) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketServer: Failed to create socket: %s\n", strerror(errno));
        return false;
    }

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;

    size_t nameLen = std::min(socketPath.size(), sizeof(addr.sun_path) - 1);
    addr.sun_path[0] = '\0';
    memcpy(addr.sun_path + 1, socketPath.data(), nameLen);
    socklen_t addrLen = static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + 1 + nameLen);
    if (SysCalls::bind(serverSocket, reinterpret_cast<struct sockaddr *>(&addr), addrLen) == -1) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketServer: Failed to bind abstract socket '%s': %s\n",
                     socketPath.c_str(), strerror(errno));
        SysCalls::close(serverSocket);
        serverSocket = -1;
        return false;
    }

    if (SysCalls::listen(serverSocket, maxConnections) == -1) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketServer: Failed to listen on socket: %s\n", strerror(errno));
        SysCalls::close(serverSocket);
        serverSocket = -1;
        return false;
    }

    serverRunning.store(true);
    shutdownRequested.store(false);

    serverThread = Thread::createFunc(serverThreadEntry, this);
    if (!serverThread) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketServer: Failed to create server thread\n");
        serverRunning.store(false);
        SysCalls::close(serverSocket);
        serverSocket = -1;
        return false;
    }

    PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                 "IpcSocketServer: Started server (abstract) name=%s\n", socketPath.c_str());
    return true;
}

void IpcSocketServer::shutdown() {
    shutdownImpl();
}

void IpcSocketServer::shutdownImpl() {
    if (!serverRunning.load()) {
        return;
    }

    shutdownRequested.store(true);

    if (serverThread) {
        serverThread->join();
        serverThread.reset();
    }

    if (serverSocket != -1) {
        SysCalls::close(serverSocket);
        serverSocket = -1;
    }

    std::lock_guard<std::mutex> lock(handleMapMutex);
    for (auto &entry : handleMap) {
        if (entry.second.fileDescriptor != -1) {
            SysCalls::close(entry.second.fileDescriptor);
        }
    }
    handleMap.clear();

    serverRunning.store(false);
    PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                 "IpcSocketServer: Server shutdown complete (abstract)\n");
}

bool IpcSocketServer::registerHandle(uint64_t handleId, int fd) {
    std::lock_guard<std::mutex> lock(handleMapMutex);

    auto it = handleMap.find(handleId);
    if (it != handleMap.end()) {
        it->second.refCount++;
        return true;
    }

    int dupFd = SysCalls::dup(fd);
    if (dupFd == -1) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketServer: Failed to duplicate fd %d: %s\n", fd, strerror(errno));
        return false;
    }

    IpcHandleEntry entry;
    entry.fileDescriptor = dupFd;
    entry.refCount = 1;

    handleMap[handleId] = entry;

    PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                 "IpcSocketServer: Registered handle %lu with fd %d\n", handleId, dupFd);
    return true;
}

bool IpcSocketServer::unregisterHandle(uint64_t handleId) {
    std::lock_guard<std::mutex> lock(handleMapMutex);

    auto it = handleMap.find(handleId);
    if (it == handleMap.end()) {
        return false;
    }

    it->second.refCount--;
    if (it->second.refCount == 0) {
        if (it->second.fileDescriptor != -1) {
            SysCalls::close(it->second.fileDescriptor);
        }
        handleMap.erase(it);
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketServer: Unregistered handle %lu\n", handleId);
    }

    return true;
}

void *IpcSocketServer::serverThreadEntry(void *arg) {
    static_cast<IpcSocketServer *>(arg)->serverThreadRun();
    return nullptr;
}

void IpcSocketServer::serverThreadRun() {
    while (!shutdownRequested.load()) {
        struct pollfd pfd = {};
        pfd.fd = serverSocket;
        pfd.events = POLLIN;

        int pollResult = SysCalls::poll(&pfd, 1, 1000); // 1 second timeout
        if (pollResult == -1) {
            if (errno != EINTR) {
                PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                             "IpcSocketServer: Poll error: %s\n", strerror(errno));
                break;
            }
            continue;
        }

        if (pollResult == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            int clientSocket = SysCalls::accept(serverSocket, nullptr, nullptr);
            if (clientSocket == -1) {
                if (errno != EAGAIN) {
                    PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                                 "IpcSocketServer: Accept error: %s\n", strerror(errno));
                }
                continue;
            }

            struct timeval timeout;
            timeout.tv_sec = socketTimeout / 1000;
            timeout.tv_usec = (socketTimeout % 1000) * 1000;

            SysCalls::setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            SysCalls::setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            handleClientConnection(clientSocket);
            SysCalls::close(clientSocket);
        }
    }
}

bool IpcSocketServer::handleClientConnection(int clientSocket) {
    IpcSocketMessage msg;
    if (!receiveMessage(clientSocket, msg)) {
        return false;
    }

    switch (msg.type) {
    case IpcSocketMessageType::requestHandle:
        return processRequestMessage(clientSocket, msg);
    default:
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcSocketServer: Unknown message type %u\n", static_cast<uint32_t>(msg.type));
        return false;
    }
}

bool IpcSocketServer::processRequestMessage(int clientSocket, const IpcSocketMessage &msg) {
    int fdToSend = -1;
    bool success = false;

    {
        std::lock_guard<std::mutex> lock(handleMapMutex);
        auto it = handleMap.find(msg.handleId);
        if (it != handleMap.end() && it->second.fileDescriptor != -1) {
            fdToSend = it->second.fileDescriptor;
            success = true;
        }
    }

    IpcSocketResponsePayload responsePayload{};
    responsePayload.success = success;

    if (success) {
        return sendFileDescriptor(clientSocket, fdToSend, &responsePayload, sizeof(responsePayload));
    } else {
        IpcSocketMessage response{};
        response.type = IpcSocketMessageType::responseHandle;
        response.processId = SysCalls::getpid();
        response.handleId = msg.handleId;
        response.payloadSize = sizeof(IpcSocketResponsePayload);
        return sendMessage(clientSocket, response, &responsePayload);
    }
}

bool IpcSocketServer::sendFileDescriptor(int socket, int fd, const void *data, size_t dataSize) {
    struct msghdr msg = {};
    struct iovec iov;
    char cmsgBuffer[CMSG_SPACE(sizeof(int))];

    iov.iov_base = const_cast<void *>(data);
    iov.iov_len = dataSize;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgBuffer;
    msg.msg_controllen = sizeof(cmsgBuffer);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *reinterpret_cast<int *>(CMSG_DATA(cmsg)) = fd;

    return SysCalls::sendmsg(socket, &msg, 0) != -1;
}

bool IpcSocketServer::sendMessage(int socket, const IpcSocketMessage &msg, const void *payload) {
    if (SysCalls::send(socket, &msg, sizeof(msg), 0) != sizeof(msg)) {
        return false;
    }

    if (payload && msg.payloadSize > 0) {
        if (SysCalls::send(socket, payload, msg.payloadSize, 0) != static_cast<ssize_t>(msg.payloadSize)) {
            return false;
        }
    }

    return true;
}

bool IpcSocketServer::receiveMessage(int socket, IpcSocketMessage &msg, void *payload, size_t maxPayloadSize) {
    if (SysCalls::recv(socket, &msg, sizeof(msg), MSG_WAITALL) != sizeof(msg)) {
        return false;
    }

    if (payload && msg.payloadSize > 0) {
        if (maxPayloadSize < msg.payloadSize) {
            return false;
        }
        if (SysCalls::recv(socket, payload, msg.payloadSize, MSG_WAITALL) != static_cast<ssize_t>(msg.payloadSize)) {
            return false;
        }
    }

    return true;
}

} // namespace NEO