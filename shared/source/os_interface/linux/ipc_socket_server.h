/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_thread.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <sys/socket.h>
#include <sys/un.h>

namespace NEO {

enum class IpcSocketMessageType : uint32_t {
    requestHandle = 1,
    responseHandle = 2
};

#pragma pack(1)
struct alignas(8) IpcSocketMessage {
    IpcSocketMessageType type;
    uint32_t processId;
    uint64_t handleId;
    uint32_t payloadSize;
    uint32_t reserved;
};

struct IpcSocketResponsePayload {
    bool success;
    uint8_t reserved[7];
};
#pragma pack()

static_assert(sizeof(IpcSocketMessage) == 24, "IpcSocketMessage size must be 24 bytes");
static_assert(sizeof(IpcSocketResponsePayload) == 8, "IpcSocketResponsePayload size must be 8 bytes");

struct IpcHandleEntry {
    int fileDescriptor = -1;
    uint64_t refCount = 0;
};

class IpcSocketServer {
  public:
    IpcSocketServer();
    virtual ~IpcSocketServer();

    virtual bool initialize();
    virtual void shutdown();

    virtual bool registerHandle(uint64_t handleId, int fd);
    virtual bool unregisterHandle(uint64_t handleId);

    virtual std::string getSocketPath() const { return socketPath; }
    virtual bool isRunning() const { return serverRunning.load(); }

  protected:
    void shutdownImpl();

    static void *serverThreadEntry(void *arg);
    void serverThreadRun();

    bool handleClientConnection(int clientSocket);
    bool processRequestMessage(int clientSocket, const IpcSocketMessage &msg);

    bool sendFileDescriptor(int socket, int fd, const void *data, size_t dataSize);

    bool sendMessage(int socket, const IpcSocketMessage &msg, const void *payload = nullptr);
    bool receiveMessage(int socket, IpcSocketMessage &msg, void *payload = nullptr, size_t maxPayloadSize = 0);

    std::atomic<bool> serverRunning{false};
    std::atomic<bool> shutdownRequested{false};
    int serverSocket = -1;
    std::mutex handleMapMutex;
    std::map<uint64_t, IpcHandleEntry> handleMap;

  private:
    std::unique_ptr<Thread> serverThread;

    std::string socketPath;

    static constexpr size_t maxConnections = 16;
    static constexpr int socketTimeout = 5000; // 5 seconds
};

} // namespace NEO